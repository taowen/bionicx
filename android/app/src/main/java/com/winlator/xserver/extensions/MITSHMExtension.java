package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Drawable;
import com.winlator.xserver.GraphicsContext;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XLock;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadDrawable;
import com.winlator.xserver.errors.BadGraphicsContext;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.BadSHMSegment;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;
import java.nio.ByteBuffer;

public class MITSHMExtension extends Extension {
    public static final byte MAJOR_VERSION = 1;
    public static final byte MINOR_VERSION = 1;

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte ATTACH = 1;
        private static final byte DETACH = 2;
        private static final byte PUT_IMAGE = 3;
        private static final byte GET_IMAGE = 4;
        private static final byte CREATE_PIXMAP = 5;
    }

    public MITSHMExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "MIT-SHM";
    }

    @Override
    public byte getFirstErrorId() {
        return Byte.MIN_VALUE;
    }

    @Override
    public byte getFirstEventId() {
        return 64;
    }

    private void queryVersion(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        if (xServer.getSHMSegmentManager() == null) throw new BadImplementation();
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeShort((short)MAJOR_VERSION);
            outputStream.writeShort((short)MINOR_VERSION);
            outputStream.writeShort((short)0);
            outputStream.writeShort((short)0);
            outputStream.writeByte((byte)0);
            // Every X11 reply is at least 32 bytes. Qt's xcb client waits for
            // the complete fixed-size QueryVersion reply before continuing.
            outputStream.writePad(15);
        }
    }

    private void attach(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int xid = inputStream.readInt();
        int shmid = inputStream.readInt();
        inputStream.skip(4);
        xServer.getSHMSegmentManager().attach(xid, shmid);
    }

    private void detach(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        xServer.getSHMSegmentManager().detach(inputStream.readInt());
    }

    private void putImage(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int drawableId = inputStream.readInt();
        int gcId = inputStream.readInt();
        short totalWidth = inputStream.readShort();
        short totalHeight = inputStream.readShort();
        short srcX = inputStream.readShort();
        short srcY = inputStream.readShort();
        short srcWidth = inputStream.readShort();
        short srcHeight = inputStream.readShort();
        short dstX = inputStream.readShort();
        short dstY = inputStream.readShort();
        byte depth = inputStream.readByte();
        inputStream.skip(3);
        int shmseg = inputStream.readInt();
        int offset = inputStream.readInt();

        Drawable drawable = xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);

        GraphicsContext graphicsContext = xServer.graphicsContextManager.getGraphicsContext(gcId);
        if (graphicsContext == null) throw new BadGraphicsContext(gcId);

        ByteBuffer segment = xServer.getSHMSegmentManager().getData(shmseg);
        if (segment == null || offset < 0 || offset >= segment.capacity())
            throw new BadSHMSegment(shmseg);
        ByteBuffer data = segment.duplicate().order(segment.order());
        data.position(offset);
        data = data.slice().order(segment.order());

        if (graphicsContext.getFunction() != GraphicsContext.Function.COPY) {
            throw new UnsupportedOperationException("GC Function other than COPY is not supported.");
        }

        if (drawable.isUseSharedData()) {
            synchronized (drawable.renderLock) {
                if (drawable.getData() != data) drawable.setData(data);
                drawable.forceUpdate();
            }
        }
        else drawable.drawImage(srcX, srcY, dstX, dstY, srcWidth, srcHeight, depth, data, totalWidth, totalHeight);
    }

    private void createPixmap(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        inputStream.skip(4);
        int drawableId = inputStream.readInt();
        short width = inputStream.readShort();
        inputStream.skip(14);

        Drawable drawable = xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);

        drawable.setUseSharedData(width == drawable.width);
    }

    private void getImage(XClient client, XInputStream inputStream,
                          XOutputStream outputStream) throws IOException, XRequestError {
        int drawableId = inputStream.readInt();
        short x = inputStream.readShort();
        short y = inputStream.readShort();
        short width = inputStream.readShort();
        short height = inputStream.readShort();
        inputStream.skip(4); // plane mask
        byte format = inputStream.readByte();
        inputStream.skip(3);
        int shmseg = inputStream.readInt();
        int offset = inputStream.readInt();

        Drawable drawable = xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        ByteBuffer target = xServer.getSHMSegmentManager().getData(shmseg);
        if (target == null) throw new BadSHMSegment(shmseg);
        // MIT-SHM format 2 is ZPixmap, the only GetImage format supported by
        // the core server and by the 32-bpp Android drawable storage.
        if (format != 2) throw new BadImplementation();

        ByteBuffer source = drawable.getImage(x, y, width, height);
        int size = source.remaining();
        if (offset < 0 || offset > target.capacity() - size)
            throw new BadSHMSegment(shmseg);
        for (int index = 0; index < size; index++)
            target.put(offset + index, source.get(index));

        int visualId = xServer.pixmapManager.getPixmap(drawableId) == null
                ? drawable.visual.id : 0;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte(drawable.visual.depth);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt(visualId);
            outputStream.writeInt(size);
            outputStream.writePad(16);
        }
    }

    @Override
    public void handleRequest(XClient client, XInputStream inputStream, XOutputStream outputStream) throws IOException, XRequestError {
        int opcode = client.getRequestData();
        switch (opcode) {
            case ClientOpcodes.QUERY_VERSION :
                queryVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.ATTACH :
                try (XLock lock = xServer.lock(XServer.Lockable.SHMSEGMENT_MANAGER)) {
                    attach(client, inputStream, outputStream);
                }
                break;
            case ClientOpcodes.DETACH :
                try (XLock lock = xServer.lock(XServer.Lockable.SHMSEGMENT_MANAGER)) {
                    detach(client, inputStream, outputStream);
                }
                break;
            case ClientOpcodes.PUT_IMAGE :
                try (XLock lock = xServer.lock(XServer.Lockable.SHMSEGMENT_MANAGER, XServer.Lockable.DRAWABLE_MANAGER, XServer.Lockable.GRAPHIC_CONTEXT_MANAGER)) {
                    putImage(client, inputStream, outputStream);
                }
                break;
            case ClientOpcodes.GET_IMAGE :
                try (XLock lock = xServer.lock(XServer.Lockable.SHMSEGMENT_MANAGER,
                        XServer.Lockable.DRAWABLE_MANAGER)) {
                    getImage(client, inputStream, outputStream);
                }
                break;
            case ClientOpcodes.CREATE_PIXMAP :
                try (XLock lock = xServer.lock(XServer.Lockable.DRAWABLE_MANAGER)) {
                    createPixmap(client, inputStream, outputStream);
                }
                break;
            default:
                throw new BadImplementation();
        }
    }
}
