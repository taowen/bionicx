package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import android.util.SparseArray;

import com.winlator.core.Callback;
import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Pixmap;
import com.winlator.xserver.Window;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadDrawable;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadValue;
import com.winlator.xserver.errors.XRequestError;
import com.winlator.xserver.events.DamageNotify;

import java.io.IOException;
import java.util.ArrayList;
import java.util.IdentityHashMap;

/** DAMAGE 1.1 Create/Add/Subtract/Destroy with DamageNotify. */
public class XDamageExtension extends Extension {
    public static final int MAJOR_VERSION = 1;
    public static final int MINOR_VERSION = 1;
    // Distinct from Shape 65, XFixes 70, RandR 72. Xlib drops unknown types.
    private static final byte FIRST_EVENT = 73;
    private static final byte FIRST_ERROR = -109;

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte CREATE = 1;
        private static final byte DESTROY = 2;
        private static final byte SUBTRACT = 3;
        private static final byte ADD = 4;
    }

    private static final class Damage {
        private final int id;
        private final int drawableId;
        private final Window window;
        private final int level;
        private final XClient client;

        private Damage(int id, int drawableId, Window window, int level,
                       XClient client) {
            this.id = id;
            this.drawableId = drawableId;
            this.window = window;
            this.level = level;
            this.client = client;
        }
    }

    private final SparseArray<Damage> damages = new SparseArray<>();
    private final IdentityHashMap<XClient, ArrayList<Integer>> clientDamages =
            new IdentityHashMap<>();
    private final Callback<XClient> onClientDestroy = this::freeClientState;

    public XDamageExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "DAMAGE";
    }

    @Override
    public byte getFirstEventId() {
        return FIRST_EVENT;
    }

    @Override
    public byte getFirstErrorId() {
        return FIRST_ERROR;
    }

    private XRequestError badDamage(int id) {
        return new XRequestError(Byte.toUnsignedInt(FIRST_ERROR), id);
    }

    private Window drawableWindow(int drawableId) throws XRequestError {
        Window window = xServer.windowManager.getWindow(drawableId);
        if (window != null) return window;
        Pixmap pixmap = xServer.pixmapManager.getPixmap(drawableId);
        if (pixmap == null) throw new BadDrawable(drawableId);
        return null;
    }

    private void queryVersion(XClient client, XInputStream inputStream,
                              XOutputStream outputStream) throws IOException {
        inputStream.skip(8);
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(MAJOR_VERSION);
            outputStream.writeInt(MINOR_VERSION);
            outputStream.writePad(16);
        }
    }

    private void create(XClient client, XInputStream inputStream)
            throws XRequestError {
        int id = inputStream.readInt();
        int drawableId = inputStream.readInt();
        int level = inputStream.readUnsignedByte();
        inputStream.skip(3);
        if (!client.isValidResourceId(id)) throw new BadIdChoice(id);
        if (level > 3) throw new BadValue(level);
        Window window = drawableWindow(drawableId);
        synchronized (damages) {
            if (damages.indexOfKey(id) >= 0) throw new BadIdChoice(id);
            damages.put(id, new Damage(id, drawableId, window, level, client));
            ArrayList<Integer> owned = clientDamages.get(client);
            if (owned == null) {
                owned = new ArrayList<>();
                clientDamages.put(client, owned);
                client.addOnDestroyListener(onClientDestroy);
            }
            owned.add(id);
        }
    }

    private void destroy(XInputStream inputStream) throws XRequestError {
        int id = inputStream.readInt();
        synchronized (damages) {
            Damage damage = damages.get(id);
            if (damage == null) throw badDamage(id);
            damages.remove(id);
            ArrayList<Integer> owned = clientDamages.get(damage.client);
            if (owned != null) owned.remove(Integer.valueOf(id));
        }
    }

    private void subtract(XInputStream inputStream) throws XRequestError {
        int id = inputStream.readInt();
        inputStream.skip(8); // repair and parts regions
        synchronized (damages) {
            if (damages.get(id) == null) throw badDamage(id);
        }
    }

    private void add(XInputStream inputStream) throws XRequestError {
        int drawableId = inputStream.readInt();
        inputStream.skip(4); // region, None means the whole drawable
        Window window = drawableWindow(drawableId);
        int width = window != null ? window.getWidth() : 1;
        int height = window != null ? window.getHeight() : 1;
        short x = window != null ? window.getX() : 0;
        short y = window != null ? window.getY() : 0;
        ArrayList<Damage> snapshot = new ArrayList<>();
        synchronized (damages) {
            for (int i = 0; i < damages.size(); i++) {
                Damage damage = damages.valueAt(i);
                if (damage.drawableId == drawableId) snapshot.add(damage);
            }
        }
        for (Damage damage : snapshot) {
            DamageNotify notify = new DamageNotify(
                    Byte.toUnsignedInt(FIRST_EVENT), damage.level, drawableId,
                    damage.id, (short)0, (short)0, width, height,
                    x, y, width, height);
            damage.client.sendEvent(notify);
            if (damage.window != null) damage.window.sendEvent(notify);
        }
    }

    private void freeClientState(XClient client) {
        synchronized (damages) {
            ArrayList<Integer> owned = clientDamages.remove(client);
            if (owned == null) return;
            for (int id : owned) damages.remove(id);
        }
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case ClientOpcodes.QUERY_VERSION:
                queryVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.CREATE:
                create(client, inputStream);
                break;
            case ClientOpcodes.DESTROY:
                destroy(inputStream);
                break;
            case ClientOpcodes.SUBTRACT:
                subtract(inputStream);
                break;
            case ClientOpcodes.ADD:
                add(inputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
