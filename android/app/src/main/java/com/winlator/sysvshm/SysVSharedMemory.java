package com.winlator.sysvshm;

import android.util.SparseArray;

import com.winlator.xconnector.XConnectorEpoll;

import java.nio.ByteBuffer;

public class SysVSharedMemory {
    private final SparseArray<SHMemory> shmemories = new SparseArray<>();
    private int maxSHMemoryId = 0;

    static {
        System.loadLibrary("winlator");
    }

    private static class SHMemory {
        private int fd;
        private long size;
        private ByteBuffer data;
    }

    public int getFd(int shmid) {
        synchronized (shmemories) {
            SHMemory shmemory = shmemories.get(shmid);
            return shmemory != null ? shmemory.fd : -1;
        }
    }

    public int get(long size) {
        synchronized (shmemories) {
            int index = shmemories.size();
            int fd = ashmemCreateRegion(index, size);
            if (fd < 0) return -1;

            SHMemory shmemory = new SHMemory();
            int id = ++maxSHMemoryId;
            shmemory.fd = fd;
            shmemory.size = size;
            shmemories.put(id, shmemory);
            return id;
        }
    }

    public void delete(int shmid) {
        synchronized (shmemories) {
            SHMemory shmemory = shmemories.get(shmid);
            if (shmemory != null) {
                if (shmemory.fd != -1) {
                    XConnectorEpoll.closeFd(shmemory.fd);
                    shmemory.fd = -1;
                }
                shmemories.remove(shmid);
            }
        }
    }

    public void deleteAll() {
        synchronized (shmemories) {
            for (int i = shmemories.size() - 1; i >= 0; i--) delete(shmemories.keyAt(i));
        }
    }

    public ByteBuffer attach(int shmid) {
        synchronized (shmemories) {
            SHMemory shmemory = shmemories.get(shmid);
            if (shmemory == null) {
                int fd = openRemoteSHMSegment(shmid);
                long size = fd >= 0 ? getMemorySize(fd) : -1;
                if (size <= 0) {
                    if (fd >= 0) XConnectorEpoll.closeFd(fd);
                    return null;
                }
                shmemory = new SHMemory();
                shmemory.fd = fd;
                shmemory.size = size;
                shmemories.put(shmid, shmemory);
            }
            if (shmemory != null) {
                if (shmemory.data == null) shmemory.data = mapSHMSegment(shmemory.fd, shmemory.size, 0, false);
                return shmemory.data;
            }
            else return null;
        }
    }

    public void detach(ByteBuffer data) {
        synchronized (shmemories) {
            for (int i = 0; i < shmemories.size(); i++) {
                SHMemory shmemory = shmemories.valueAt(i);
                if (shmemory.data == data) {
                    if (shmemory.data != null) {
                        unmapSHMSegment(shmemory.data, shmemory.size);
                        shmemory.data = null;
                    }
                    break;
                }
            }
        }
    }

    public static native int createMemoryFd(String name, int size);

    private static native int ashmemCreateRegion(int index, long size);

    private static native int openRemoteSHMSegment(int shmid);

    private static native long getMemorySize(int fd);

    public static native ByteBuffer mapSHMSegment(int fd, long size, int offset, boolean readonly);

    public static native void unmapSHMSegment(ByteBuffer data, long size);
}
