#ifndef VORTEK_TIMELINE_SEMAPHORE_H
#define VORTEK_TIMELINE_SEMAPHORE_H

#include "vortek.h"

extern void TimelineSemaphore_asyncWait(int clientFd, ThreadPool* threadPool, char* inputBuffer, int inputBufferSize);
extern VkResult TimelineSemaphore_create(VkDevice device, VkSemaphoreCreateInfo* createInfo,
                                         bool hostTimeline, VkSemaphore* semaphore);
extern void TimelineSemaphore_destroy(VkDevice device, VkSemaphore semaphore);
extern VkResult TimelineSemaphore_signal(VkDevice device, const VkSemaphoreSignalInfo* signalInfo);
extern VkResult TimelineSemaphore_counter(VkDevice device, VkSemaphore semaphore, uint64_t* value);
extern bool TimelineSemaphore_filterSubmits(VkSubmitInfo* submits, uint32_t submitCount);
extern bool TimelineSemaphore_filterSubmits2(VkSubmitInfo2* submits, uint32_t submitCount);
extern void TimelineSemaphore_flushSubmitSignals(void);
/* WSI acquire is an RPC: the image is ready when the call returns.
 * Binary acquire semaphores are marked here and dropped from later
 * host QueueSubmit waits. Emulated timelines are incremented. Empty
 * vkQueueSubmit is not used (Adreno faults on some proxy handles). */
extern void TimelineSemaphore_rpcSignal(VkSemaphore semaphore);

#endif
