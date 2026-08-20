#include <sys/eventfd.h>

#include "timeline_semaphore.h"
#include "vortek_serializer.h"
#include "string_utils.h"
#include "vulkan_helper.h"

typedef struct EmulatedTimeline {
    VkSemaphore semaphore;
    uint64_t value;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} EmulatedTimeline;

static pthread_mutex_t emulatedLock = PTHREAD_MUTEX_INITIALIZER;
static ArrayList emulatedTimelines;
static EmulatedTimeline* emulatedFind(VkSemaphore semaphore);

#define MAX_PENDING_SUBMIT_SIGNALS 32
static VkSemaphore pendingSubmitSemaphores[MAX_PENDING_SUBMIT_SIGNALS];
static uint64_t pendingSubmitValues[MAX_PENDING_SUBMIT_SIGNALS];
static uint32_t pendingSubmitCount;

#define MAX_RPC_SIGNALED 64
static VkSemaphore rpcSignaled[MAX_RPC_SIGNALED];
static uint32_t rpcSignaledCount;

static bool consumeRpcSignaledLocked(VkSemaphore semaphore) {
    uint32_t i;

    if (!semaphore) return false;
    for (i = 0; i < rpcSignaledCount; i++) {
        if (rpcSignaled[i] != semaphore) continue;
        rpcSignaled[i] = rpcSignaled[--rpcSignaledCount];
        return true;
    }
    return false;
}

static bool waitSatisfiedLocked(VkSemaphore semaphore) {
    if (!semaphore) return false;
    if (emulatedFind(semaphore)) return true;
    return consumeRpcSignaledLocked(semaphore);
}

static void stashEmulatedSignal(VkSemaphore semaphore, uint64_t value) {
    if (pendingSubmitCount >= MAX_PENDING_SUBMIT_SIGNALS) return;
    pendingSubmitSemaphores[pendingSubmitCount] = semaphore;
    pendingSubmitValues[pendingSubmitCount] = value;
    pendingSubmitCount++;
}

static EmulatedTimeline* emulatedFind(VkSemaphore semaphore) {
    for (int i = 0; i < emulatedTimelines.size; i++) {
        EmulatedTimeline* timeline = emulatedTimelines.elements[i];
        if (timeline->semaphore == semaphore) return timeline;
    }
    return NULL;
}

static void emulatedRegister(VkSemaphore semaphore, uint64_t initial) {
    EmulatedTimeline* timeline = calloc(1, sizeof(*timeline));
    if (!timeline) return;
    timeline->semaphore = semaphore;
    timeline->value = initial;
    pthread_mutex_init(&timeline->mutex, NULL);
    pthread_cond_init(&timeline->cond, NULL);
    pthread_mutex_lock(&emulatedLock);
    ArrayList_add(&emulatedTimelines, timeline);
    pthread_mutex_unlock(&emulatedLock);
}

static void emulatedUnregister(VkSemaphore semaphore) {
    pthread_mutex_lock(&emulatedLock);
    for (int i = 0; i < emulatedTimelines.size; i++) {
        EmulatedTimeline* timeline = emulatedTimelines.elements[i];
        if (timeline->semaphore != semaphore) continue;
        ArrayList_removeAt(&emulatedTimelines, i);
        pthread_mutex_unlock(&emulatedLock);
        pthread_mutex_destroy(&timeline->mutex);
        pthread_cond_destroy(&timeline->cond);
        free(timeline);
        return;
    }
    pthread_mutex_unlock(&emulatedLock);
}

static bool isTimelineCreate(const VkSemaphoreCreateInfo* createInfo, uint64_t* initial) {
    VkSemaphoreTypeCreateInfo* typeInfo = findNextVkStructure(
            (void*)createInfo->pNext, VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO);
    if (!typeInfo || typeInfo->semaphoreType != VK_SEMAPHORE_TYPE_TIMELINE)
        return false;
    if (initial) *initial = typeInfo->initialValue;
    return true;
}

VkResult TimelineSemaphore_create(VkDevice device, VkSemaphoreCreateInfo* createInfo,
                                  bool hostTimeline, VkSemaphore* semaphore) {
    uint64_t initial = 0;
    bool wantTimeline = isTimelineCreate(createInfo, &initial);
    if (wantTimeline && !hostTimeline) {
        VkSemaphoreCreateInfo binary = *createInfo;
        binary.pNext = NULL;
        VkResult result = vulkanWrapper.vkCreateSemaphore(device, &binary, NULL, semaphore);
        if (result == VK_SUCCESS) emulatedRegister(*semaphore, initial);
        return result;
    }
    return vulkanWrapper.vkCreateSemaphore(device, createInfo, NULL, semaphore);
}

void TimelineSemaphore_destroy(VkDevice device, VkSemaphore semaphore) {
    emulatedUnregister(semaphore);
    vulkanWrapper.vkDestroySemaphore(device, semaphore, NULL);
}

VkResult TimelineSemaphore_signal(VkDevice device, const VkSemaphoreSignalInfo* signalInfo) {
    pthread_mutex_lock(&emulatedLock);
    EmulatedTimeline* timeline = emulatedFind(signalInfo->semaphore);
    pthread_mutex_unlock(&emulatedLock);
    if (!timeline) return vulkanWrapper.vkSignalSemaphore(device, signalInfo);
    pthread_mutex_lock(&timeline->mutex);
    if (signalInfo->value > timeline->value) timeline->value = signalInfo->value;
    pthread_cond_broadcast(&timeline->cond);
    pthread_mutex_unlock(&timeline->mutex);
    return VK_SUCCESS;
}

VkResult TimelineSemaphore_counter(VkDevice device, VkSemaphore semaphore, uint64_t* value) {
    pthread_mutex_lock(&emulatedLock);
    EmulatedTimeline* timeline = emulatedFind(semaphore);
    pthread_mutex_unlock(&emulatedLock);
    if (!timeline) return vulkanWrapper.vkGetSemaphoreCounterValue(device, semaphore, value);
    pthread_mutex_lock(&timeline->mutex);
    *value = timeline->value;
    pthread_mutex_unlock(&timeline->mutex);
    return VK_SUCCESS;
}

void TimelineSemaphore_rpcSignal(VkSemaphore semaphore) {
    EmulatedTimeline* timeline;

    if (!semaphore) return;
    pthread_mutex_lock(&emulatedLock);
    timeline = emulatedFind(semaphore);
    if (timeline) {
        pthread_mutex_unlock(&emulatedLock);
        pthread_mutex_lock(&timeline->mutex);
        timeline->value++;
        pthread_cond_broadcast(&timeline->cond);
        pthread_mutex_unlock(&timeline->mutex);
        return;
    }
    for (uint32_t i = 0; i < rpcSignaledCount; i++) {
        if (rpcSignaled[i] == semaphore) {
            pthread_mutex_unlock(&emulatedLock);
            return;
        }
    }
    if (rpcSignaledCount < MAX_RPC_SIGNALED)
        rpcSignaled[rpcSignaledCount++] = semaphore;
    pthread_mutex_unlock(&emulatedLock);
}

void TimelineSemaphore_flushSubmitSignals(void) {
    VkSemaphore semaphores[MAX_PENDING_SUBMIT_SIGNALS];
    uint64_t values[MAX_PENDING_SUBMIT_SIGNALS];
    pthread_mutex_lock(&emulatedLock);
    uint32_t count = pendingSubmitCount;
    memcpy(semaphores, pendingSubmitSemaphores, count * sizeof(VkSemaphore));
    memcpy(values, pendingSubmitValues, count * sizeof(uint64_t));
    pendingSubmitCount = 0;
    pthread_mutex_unlock(&emulatedLock);
    for (uint32_t i = 0; i < count; i++) {
        VkSemaphoreSignalInfo info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .semaphore = semaphores[i],
            .value = values[i],
        };
        TimelineSemaphore_signal(VK_NULL_HANDLE, &info);
    }
}

bool TimelineSemaphore_filterSubmits(VkSubmitInfo* submits, uint32_t submitCount) {
    bool hostWork = false;
    pthread_mutex_lock(&emulatedLock);
    for (uint32_t s = 0; s < submitCount; s++) {
        VkSubmitInfo* submit = &submits[s];
        uint32_t kept = 0;
        for (uint32_t i = 0; i < submit->waitSemaphoreCount; i++) {
            if (waitSatisfiedLocked(submit->pWaitSemaphores[i])) continue;
            if (kept != i) {
                ((VkSemaphore*)submit->pWaitSemaphores)[kept] = submit->pWaitSemaphores[i];
                if (submit->pWaitDstStageMask)
                    ((VkPipelineStageFlags*)submit->pWaitDstStageMask)[kept] =
                            submit->pWaitDstStageMask[i];
            }
            kept++;
        }
        submit->waitSemaphoreCount = kept;
        kept = 0;
        VkTimelineSemaphoreSubmitInfo* timelineInfo = findNextVkStructure(
                (void*)submit->pNext, VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO);
        for (uint32_t i = 0; i < submit->signalSemaphoreCount; i++) {
            if (emulatedFind(submit->pSignalSemaphores[i])) {
                uint64_t value = 0;
                if (timelineInfo && i < timelineInfo->signalSemaphoreValueCount)
                    value = timelineInfo->pSignalSemaphoreValues[i];
                stashEmulatedSignal(submit->pSignalSemaphores[i], value);
                continue;
            }
            if (kept != i)
                ((VkSemaphore*)submit->pSignalSemaphores)[kept] = submit->pSignalSemaphores[i];
            kept++;
        }
        submit->signalSemaphoreCount = kept;
        if (submit->waitSemaphoreCount == 0 && submit->signalSemaphoreCount == 0)
            submit->pNext = removeNextVkStructure(
                    (void*)submit->pNext,
                    VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO);
        if (submit->waitSemaphoreCount || submit->commandBufferCount
                || submit->signalSemaphoreCount)
            hostWork = true;
    }
    pthread_mutex_unlock(&emulatedLock);
    return hostWork;
}

bool TimelineSemaphore_filterSubmits2(VkSubmitInfo2* submits, uint32_t submitCount) {
    bool hostWork = false;
    pthread_mutex_lock(&emulatedLock);
    for (uint32_t s = 0; s < submitCount; s++) {
        VkSubmitInfo2* submit = &submits[s];
        uint32_t kept = 0;
        VkSemaphoreSubmitInfo* waits =
                (VkSemaphoreSubmitInfo*)submit->pWaitSemaphoreInfos;
        for (uint32_t i = 0; i < submit->waitSemaphoreInfoCount; i++) {
            if (!waits || waitSatisfiedLocked(waits[i].semaphore)) continue;
            if (kept != i) waits[kept] = waits[i];
            kept++;
        }
        submit->waitSemaphoreInfoCount = kept;
        kept = 0;
        VkSemaphoreSubmitInfo* signals =
                (VkSemaphoreSubmitInfo*)submit->pSignalSemaphoreInfos;
        for (uint32_t i = 0; i < submit->signalSemaphoreInfoCount; i++) {
            if (signals && emulatedFind(signals[i].semaphore)) {
                stashEmulatedSignal(signals[i].semaphore, signals[i].value);
                continue;
            }
            if (signals && kept != i) signals[kept] = signals[i];
            kept++;
        }
        submit->signalSemaphoreInfoCount = kept;
        if (submit->waitSemaphoreInfoCount || submit->commandBufferInfoCount
                || submit->signalSemaphoreInfoCount)
            hostWork = true;
    }
    pthread_mutex_unlock(&emulatedLock);
    return hostWork;
}

typedef struct WaitSemaphoresRequest {
    int notifyFd;
    char* inputBuffer;
} WaitSemaphoresRequest;

static void waitSemaphoresThread(void* param) {
    WaitSemaphoresRequest* waitSemaphoresRequest = param;

    uint64_t deviceId;
    VkSemaphoreWaitInfo waitInfo = {0};
    uint64_t timeout;

    MemoryPool memoryPool = {0};
    vt_unserialize_vkWaitSemaphores((VkDevice)&deviceId, &waitInfo, &timeout, waitSemaphoresRequest->inputBuffer, &memoryPool);
    VkDevice device = VkObject_fromId(deviceId);

    VkResult result = VK_SUCCESS;
    bool allEmulated = waitInfo.semaphoreCount > 0;
    pthread_mutex_lock(&emulatedLock);
    for (uint32_t i = 0; i < waitInfo.semaphoreCount; i++) {
        if (!emulatedFind(waitInfo.pSemaphores[i])) {
            allEmulated = false;
            break;
        }
    }
    pthread_mutex_unlock(&emulatedLock);
    if (allEmulated) {
        for (uint32_t i = 0; i < waitInfo.semaphoreCount && result == VK_SUCCESS; i++) {
            pthread_mutex_lock(&emulatedLock);
            EmulatedTimeline* timeline = emulatedFind(waitInfo.pSemaphores[i]);
            pthread_mutex_unlock(&emulatedLock);
            if (!timeline) {
                result = VK_ERROR_DEVICE_LOST;
                break;
            }
            pthread_mutex_lock(&timeline->mutex);
            while (timeline->value < waitInfo.pValues[i])
                pthread_cond_wait(&timeline->cond, &timeline->mutex);
            pthread_mutex_unlock(&timeline->mutex);
        }
    } else {
        result = vulkanWrapper.vkWaitSemaphores(device, &waitInfo, timeout);
    }

    uint64_t value = result == VK_SUCCESS ? 1 : (result == VK_ERROR_DEVICE_LOST ? 2 : 3);
    write(waitSemaphoresRequest->notifyFd, &value, sizeof(uint64_t));
    CLOSEFD(waitSemaphoresRequest->notifyFd);

    vt_free(&memoryPool);
}

void TimelineSemaphore_asyncWait(int clientFd, ThreadPool* threadPool, char* inputBuffer, int inputBufferSize) {
    WaitSemaphoresRequest* waitSemaphoresRequest = calloc(1, sizeof(WaitSemaphoresRequest));
    waitSemaphoresRequest->inputBuffer = memdup(inputBuffer, inputBufferSize);

    int fd = eventfd(0, 0);
    waitSemaphoresRequest->notifyFd = fd;

    send_fds(clientFd, &fd, 1, NULL, 0);
    ThreadPool_run(threadPool, waitSemaphoresThread, waitSemaphoresRequest);
}
