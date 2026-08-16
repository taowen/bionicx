package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import android.util.SparseArray;

import com.winlator.core.Callback;
import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadMatch;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;
import java.util.ArrayList;
import java.util.IdentityHashMap;

/** SYNC 3.1 counters plus the existing fence opcodes. */
public class SyncExtension extends Extension {
    public static final byte MAJOR_VERSION = 3;
    public static final byte MINOR_VERSION = 1;
    public static final byte FIRST_ERROR = -126;
    public static final byte FIRST_EVENT = 74;

    private static abstract class ClientOpcodes {
        private static final byte INITIALIZE = 0;
        private static final byte LIST_SYSTEM_COUNTERS = 1;
        private static final byte CREATE_COUNTER = 2;
        private static final byte SET_COUNTER = 3;
        private static final byte CHANGE_COUNTER = 4;
        private static final byte QUERY_COUNTER = 5;
        private static final byte DESTROY_COUNTER = 6;
        private static final byte AWAIT = 7;
        private static final byte CREATE_ALARM = 8;
        private static final byte CHANGE_ALARM = 9;
        private static final byte QUERY_ALARM = 10;
        private static final byte DESTROY_ALARM = 11;
        private static final byte SET_PRIORITY = 12;
        private static final byte GET_PRIORITY = 13;
        private static final byte CREATE_FENCE = 14;
        private static final byte TRIGGER_FENCE = 15;
        private static final byte RESET_FENCE = 16;
        private static final byte DESTROY_FENCE = 17;
        private static final byte QUERY_FENCE = 18;
        private static final byte AWAIT_FENCE = 19;
    }

    private static final class Counter {
        final XClient owner;
        long previous;
        long value;

        private Counter(XClient owner, long value) {
            this.owner = owner;
            this.previous = value;
            this.value = value;
        }
    }

    private static final class Waiter {
        final XClient client;
        final int[] counters;
        final long[] waits;
        final int[] tests;
        final int[] fences;

        private Waiter(XClient client, int[] counters, long[] waits, int[] tests,
                       int[] fences) {
            this.client = client;
            this.counters = counters;
            this.waits = waits;
            this.tests = tests;
            this.fences = fences;
        }
    }

    private static final class Alarm {
        final XClient owner;
        final int id;
        int counterId;
        int valueType;
        long wait;
        int test = 2;
        long delta;
        boolean events = true;
        boolean inactive;

        private Alarm(XClient owner, int id) {
            this.owner = owner;
            this.id = id;
        }
    }

    private final SparseArray<Counter> counters = new SparseArray<>();
    private final SparseArray<Alarm> alarms = new SparseArray<>();
    private final SparseArray<Boolean> fences = new SparseArray<>();
    private final ArrayList<Waiter> waiters = new ArrayList<>();
    private final IdentityHashMap<XClient, Boolean> awaiting =
            new IdentityHashMap<>();
    private final Callback<XClient> onClientDestroy = this::freeClientState;

    public SyncExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "SYNC";
    }

    @Override
    public byte getFirstEventId() {
        return FIRST_EVENT;
    }

    public byte getFirstErrorId() {
        return FIRST_ERROR;
    }

    public boolean isAwaiting(XClient client) {
        synchronized (this) {
            return awaiting.containsKey(client);
        }
    }

    public void setTriggered(int id) {
        synchronized (this) {
            if (fences.indexOfKey(id) >= 0) fences.put(id, true);
            releaseWaiters();
        }
    }

    private XRequestError badCounter(int id) {
        return new XRequestError(Byte.toUnsignedInt(FIRST_ERROR), id);
    }

    private XRequestError badAlarm(int id) {
        return new XRequestError(Byte.toUnsignedInt(FIRST_ERROR) + 1, id);
    }

    private XRequestError badFence(int id) {
        return new XRequestError(Byte.toUnsignedInt(FIRST_ERROR) + 2, id);
    }

    private static long readValue(XInputStream input) {
        int hi = input.readInt();
        int lo = input.readInt();
        return ((long)hi << 32) | (lo & 0xffffffffL);
    }

    private static void writeValue(XOutputStream output, long value)
            throws IOException {
        output.writeInt((int)(value >> 32));
        output.writeInt((int)value);
    }

    private boolean idBusy(int id) {
        return counters.indexOfKey(id) >= 0 || alarms.indexOfKey(id) >= 0
                || fences.indexOfKey(id) >= 0;
    }

    private void initialize(XClient client, XInputStream inputStream,
                            XOutputStream outputStream) throws IOException {
        inputStream.skip(4);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeByte(MAJOR_VERSION);
            outputStream.writeByte(MINOR_VERSION);
            outputStream.writeShort((short)0);
            outputStream.writePad(20);
        }
    }

    private void listSystemCounters(XClient client, XOutputStream outputStream)
            throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(0);
            outputStream.writePad(20);
        }
    }

    private void createCounter(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        long value = readValue(inputStream);
        if (idBusy(id)) throw new BadIdChoice(id);
        counters.put(id, new Counter(client, value));
        client.addOnDestroyListener(onClientDestroy);
    }

    private void setCounter(XInputStream inputStream) throws XRequestError {
        int id = inputStream.readInt();
        Counter counter = counters.get(id);
        if (counter == null) throw badCounter(id);
        long next = readValue(inputStream);
        counter.previous = counter.value;
        counter.value = next;
        if (counter.previous != next) {
            releaseWaiters();
            fireAlarms();
        }
    }

    private void changeCounter(XInputStream inputStream) throws XRequestError {
        int id = inputStream.readInt();
        Counter counter = counters.get(id);
        if (counter == null) throw badCounter(id);
        long amount = readValue(inputStream);
        if (amount == 0) return;
        counter.previous = counter.value;
        counter.value += amount;
        releaseWaiters();
        fireAlarms();
    }

    private void queryCounter(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        int id = inputStream.readInt();
        Counter counter = counters.get(id);
        if (counter == null) throw badCounter(id);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            writeValue(outputStream, counter.value);
            outputStream.writePad(16);
        }
    }

    private void destroyCounter(XInputStream inputStream) throws XRequestError {
        int id = inputStream.readInt();
        if (counters.indexOfKey(id) < 0) throw badCounter(id);
        counters.delete(id);
    }

    private boolean counterSatisfied(int id, long wait, int test) {
        Counter counter = counters.get(id);
        if (counter == null) return false;
        if (test == 0) return counter.previous < wait && counter.value >= wait;
        if (test == 1) return counter.previous > wait && counter.value <= wait;
        if (test == 2) return counter.value >= wait;
        if (test == 3) return counter.value <= wait;
        return false;
    }

    private void await(XClient client, XInputStream inputStream)
            throws XRequestError {
        int remaining = client.getRemainingRequestLength();
        int count = remaining / 20;
        int[] ids = new int[count];
        long[] waits = new long[count];
        int[] tests = new int[count];
        boolean ready = false;
        for (int i = 0; i < count; i++) {
            ids[i] = inputStream.readInt();
            int valueType = inputStream.readInt();
            long wait = readValue(inputStream);
            tests[i] = inputStream.readInt();
            Counter counter = counters.get(ids[i]);
            if (counter == null) throw badCounter(ids[i]);
            if (valueType == 1) wait = counter.value + wait;
            waits[i] = wait;
            if (counterSatisfied(ids[i], wait, tests[i])) ready = true;
        }
        inputStream.skip(client.getRemainingRequestLength());
        if (ready || count == 0) return;
        waiters.add(new Waiter(client, ids, waits, tests, null));
        awaiting.put(client, Boolean.TRUE);
    }

    private void createFence(XInputStream inputStream) throws XRequestError {
        inputStream.skip(4);
        int id = inputStream.readInt();
        if (idBusy(id)) throw new BadIdChoice(id);
        boolean initiallyTriggered = inputStream.readByte() == 1;
        inputStream.skip(3);
        fences.put(id, initiallyTriggered);
    }

    private void triggerFence(XInputStream inputStream) throws XRequestError {
        int id = inputStream.readInt();
        if (fences.indexOfKey(id) < 0) throw badFence(id);
        fences.put(id, true);
        releaseWaiters();
    }

    private void resetFence(XInputStream inputStream) throws XRequestError {
        int id = inputStream.readInt();
        if (fences.indexOfKey(id) < 0) throw badFence(id);
        if (!fences.get(id)) throw new BadMatch();
        fences.put(id, false);
    }

    private void destroyFence(XInputStream inputStream) throws XRequestError {
        int id = inputStream.readInt();
        if (fences.indexOfKey(id) < 0) throw badFence(id);
        fences.delete(id);
    }

    private void queryFence(XClient client, XInputStream inputStream,
                            XOutputStream outputStream)
            throws IOException, XRequestError {
        int id = inputStream.readInt();
        int index = fences.indexOfKey(id);
        if (index < 0) throw badFence(id);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)(fences.valueAt(index) ? 1 : 0));
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writePad(24);
        }
    }

    private void awaitFence(XClient client, XInputStream inputStream)
            throws XRequestError {
        int remaining = client.getRemainingRequestLength();
        int[] ids = new int[remaining / 4];
        boolean ready = false;
        for (int i = 0; i < ids.length; i++) {
            ids[i] = inputStream.readInt();
            int index = fences.indexOfKey(ids[i]);
            if (index < 0) throw badFence(ids[i]);
            if (fences.valueAt(index)) ready = true;
        }
        if (ready || ids.length == 0) return;
        waiters.add(new Waiter(client, null, null, null, ids));
        awaiting.put(client, Boolean.TRUE);
    }

    private boolean waiterReady(Waiter waiter) {
        if (waiter.fences != null) {
            for (int id : waiter.fences) {
                int index = fences.indexOfKey(id);
                if (index >= 0 && fences.valueAt(index)) return true;
            }
            return false;
        }
        for (int i = 0; i < waiter.counters.length; i++) {
            if (counterSatisfied(waiter.counters[i], waiter.waits[i],
                    waiter.tests[i]))
                return true;
        }
        return false;
    }

    private void releaseWaiters() {
        boolean woke = false;
        for (int i = waiters.size() - 1; i >= 0; i--) {
            Waiter waiter = waiters.get(i);
            if (!waiterReady(waiter)) continue;
            waiters.remove(i);
            awaiting.remove(waiter.client);
            woke = true;
        }
        if (woke) xServer.pumpDeferredRequests();
    }

    private void applyAlarmMask(Alarm alarm, int mask, XInputStream input)
            throws XRequestError {
        if ((mask & 1) != 0) alarm.counterId = input.readInt();
        if ((mask & 2) != 0) alarm.valueType = input.readInt();
        if ((mask & 4) != 0) alarm.wait = readValue(input);
        if ((mask & 8) != 0) alarm.test = input.readInt();
        if ((mask & 16) != 0) alarm.delta = readValue(input);
        if ((mask & 32) != 0) alarm.events = input.readInt() != 0;
        if ((mask & ~63) != 0)
            throw new com.winlator.xserver.errors.BadValue(mask);
        if (alarm.valueType == 1) {
            Counter counter = counters.get(alarm.counterId);
            if (counter != null) alarm.wait = counter.value + alarm.wait;
            alarm.valueType = 0;
        }
    }

    private void sendAlarmNotify(Alarm alarm) {
        try {
            XOutputStream output = alarm.owner.getOutputStream();
            Counter counter = counters.get(alarm.counterId);
            long value = counter != null ? counter.value : alarm.wait;
            try (XStreamLock lock = output.lock()) {
                output.writeByte((byte)(FIRST_EVENT + 1));
                output.writeByte((byte)0);
                output.writeShort(alarm.owner.getSequenceNumber());
                output.writeInt((int)System.currentTimeMillis());
                output.writeInt(alarm.id);
                writeValue(output, value);
                writeValue(output, alarm.wait);
                output.writeByte((byte)(alarm.inactive ? 1 : 0));
                output.writePad(3);
            }
        }
        catch (IOException ignored) {
        }
    }

    private void fireAlarm(Alarm alarm) {
        if (alarm.inactive) return;
        if (!counterSatisfied(alarm.counterId, alarm.wait, alarm.test)) return;
        if (alarm.events) sendAlarmNotify(alarm);
        alarm.wait += alarm.delta;
        if (alarm.delta == 0
                || counterSatisfied(alarm.counterId, alarm.wait, alarm.test))
            alarm.inactive = true;
    }

    private void fireAlarms() {
        for (int i = 0; i < alarms.size(); i++) fireAlarm(alarms.valueAt(i));
    }

    private void createAlarm(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        int mask = inputStream.readInt();
        if (idBusy(id)) throw new BadIdChoice(id);
        Alarm alarm = new Alarm(client, id);
        applyAlarmMask(alarm, mask, inputStream);
        inputStream.skip(client.getRemainingRequestLength());
        if (counters.get(alarm.counterId) == null)
            throw badCounter(alarm.counterId);
        alarms.put(id, alarm);
        client.addOnDestroyListener(onClientDestroy);
        fireAlarm(alarm);
    }

    private void changeAlarm(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        Alarm alarm = alarms.get(id);
        if (alarm == null) throw badAlarm(id);
        applyAlarmMask(alarm, inputStream.readInt(), inputStream);
        inputStream.skip(client.getRemainingRequestLength());
        alarm.inactive = false;
        fireAlarm(alarm);
    }

    private void queryAlarm(XClient client, XInputStream inputStream,
                            XOutputStream outputStream)
            throws IOException, XRequestError {
        int id = inputStream.readInt();
        Alarm alarm = alarms.get(id);
        if (alarm == null) throw badAlarm(id);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(2);
            outputStream.writeInt(alarm.counterId);
            outputStream.writeInt(alarm.valueType);
            writeValue(outputStream, alarm.wait);
            outputStream.writeInt(alarm.test);
            writeValue(outputStream, alarm.delta);
            outputStream.writeByte((byte)(alarm.events ? 1 : 0));
            outputStream.writePad(3);
        }
    }

    private void destroyAlarm(XInputStream inputStream) throws XRequestError {
        int id = inputStream.readInt();
        if (alarms.indexOfKey(id) < 0) throw badAlarm(id);
        alarms.delete(id);
    }

    private void setPriority(XClient client) {
        client.skipRequest();
    }

    private void getPriority(XClient client, XInputStream inputStream,
                             XOutputStream outputStream) throws IOException {
        inputStream.skip(client.getRemainingRequestLength());
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(0);
            outputStream.writePad(20);
        }
    }

    private void freeClientState(XClient client) {
        synchronized (this) {
            awaiting.remove(client);
            for (int i = waiters.size() - 1; i >= 0; i--) {
                if (waiters.get(i).client == client) waiters.remove(i);
            }
            for (int i = alarms.size() - 1; i >= 0; i--) {
                if (alarms.valueAt(i).owner == client) alarms.removeAt(i);
            }
            for (int i = counters.size() - 1; i >= 0; i--) {
                if (counters.valueAt(i).owner == client)
                    counters.removeAt(i);
            }
        }
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        synchronized (this) {
        int opcode = client.getRequestData();
        switch (opcode) {
            case ClientOpcodes.INITIALIZE:
                initialize(client, inputStream, outputStream);
                break;
            case ClientOpcodes.LIST_SYSTEM_COUNTERS:
                listSystemCounters(client, outputStream);
                break;
            case ClientOpcodes.CREATE_COUNTER:
                createCounter(client, inputStream);
                break;
            case ClientOpcodes.SET_COUNTER:
                setCounter(inputStream);
                break;
            case ClientOpcodes.CHANGE_COUNTER:
                changeCounter(inputStream);
                break;
            case ClientOpcodes.QUERY_COUNTER:
                queryCounter(client, inputStream, outputStream);
                break;
            case ClientOpcodes.DESTROY_COUNTER:
                destroyCounter(inputStream);
                break;
            case ClientOpcodes.AWAIT:
                await(client, inputStream);
                break;
            case ClientOpcodes.CREATE_ALARM:
                createAlarm(client, inputStream);
                break;
            case ClientOpcodes.CHANGE_ALARM:
                changeAlarm(client, inputStream);
                break;
            case ClientOpcodes.QUERY_ALARM:
                queryAlarm(client, inputStream, outputStream);
                break;
            case ClientOpcodes.DESTROY_ALARM:
                destroyAlarm(inputStream);
                break;
            case ClientOpcodes.SET_PRIORITY:
                setPriority(client);
                break;
            case ClientOpcodes.GET_PRIORITY:
                getPriority(client, inputStream, outputStream);
                break;
            case ClientOpcodes.CREATE_FENCE:
                createFence(inputStream);
                break;
            case ClientOpcodes.TRIGGER_FENCE:
                triggerFence(inputStream);
                break;
            case ClientOpcodes.RESET_FENCE:
                resetFence(inputStream);
                break;
            case ClientOpcodes.DESTROY_FENCE:
                destroyFence(inputStream);
                break;
            case ClientOpcodes.QUERY_FENCE:
                queryFence(client, inputStream, outputStream);
                break;
            case ClientOpcodes.AWAIT_FENCE:
                awaitFence(client, inputStream);
                break;
            default:
                throw new BadImplementation();
        }
        }
    }
}
