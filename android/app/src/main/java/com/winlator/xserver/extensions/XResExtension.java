package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Atom;
import com.winlator.xserver.XClient;
import com.winlator.xserver.XLock;
import com.winlator.xserver.XServer;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.XRequestError;

import java.io.IOException;

/** X-Resource 1.2 QueryClients / pixmap-byte accounting for xfwm4. */
public class XResExtension extends Extension {
    public static final int MAJOR_VERSION = 1;
    public static final int MINOR_VERSION = 0;

    private static abstract class ClientOpcodes {
        private static final byte QUERY_VERSION = 0;
        private static final byte QUERY_CLIENTS = 1;
        private static final byte QUERY_CLIENT_RESOURCES = 2;
        private static final byte QUERY_CLIENT_PIXMAP_BYTES = 3;
        private static final byte QUERY_CLIENT_IDS = 4;
        private static final byte QUERY_RESOURCE_BYTES = 5;
    }

    public XResExtension(XServer xServer, byte majorOpcode) {
        super(xServer, majorOpcode);
    }

    @Override
    public String getName() {
        return "X-Resource";
    }

    private XClient clientForXid(int xid) {
        if (xid == 0) return null;
        for (XClient client : xServer.getClientsSnapshot()) {
            if (client.isValidResourceId(xid)) return client;
        }
        return null;
    }

    private void queryVersion(XClient client, XInputStream inputStream,
                              XOutputStream outputStream) throws IOException {
        inputStream.skip(4);
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

    private void queryClients(XClient client, XOutputStream outputStream)
            throws IOException {
        XClient[] clients = xServer.getClientsSnapshot();
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(clients.length * 2);
            outputStream.writeInt(clients.length);
            outputStream.writePad(20);
            for (XClient other : clients) {
                outputStream.writeInt(other.resourceIDBase);
                outputStream.writeInt(xServer.resourceIDs.idMask);
            }
        }
    }

    private void queryClientResources(XClient client, XInputStream inputStream,
                                      XOutputStream outputStream)
            throws IOException {
        int xid = inputStream.readInt();
        XClient owner = clientForXid(xid);
        int pixmapCount = 0;
        if (owner != null) {
            pixmapCount = xServer.pixmapManager.countForClient(owner.resourceIDBase,
                    xServer.resourceIDs.idMask);
        }
        int nTypes = pixmapCount > 0 ? 1 : 0;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(nTypes * 2);
            outputStream.writeInt(nTypes);
            outputStream.writePad(20);
            if (nTypes > 0) {
                outputStream.writeInt(Atom.internAtom("PIXMAP"));
                outputStream.writeInt(pixmapCount);
            }
        }
    }

    private void queryClientPixmapBytes(XClient client, XInputStream inputStream,
                                        XOutputStream outputStream)
            throws IOException {
        int xid = inputStream.readInt();
        XClient owner = clientForXid(xid);
        long bytes = owner != null
                ? xServer.pixmapManager.bytesForClient(owner.resourceIDBase,
                        xServer.resourceIDs.idMask)
                : 0;
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(RESPONSE_CODE_SUCCESS);
            outputStream.writeByte((byte)0);
            outputStream.writeShort(client.getSequenceNumber());
            outputStream.writeInt(0);
            outputStream.writeInt((int)bytes);
            outputStream.writeInt((int)(bytes >>> 32));
            outputStream.writePad(16);
        }
    }

    private void emptyList(XClient client, XInputStream inputStream,
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

    @Override
    public void handleRequest(XClient client, XInputStream inputStream,
                              XOutputStream outputStream)
            throws IOException, XRequestError {
        switch (client.getRequestData()) {
            case ClientOpcodes.QUERY_VERSION:
                queryVersion(client, inputStream, outputStream);
                break;
            case ClientOpcodes.QUERY_CLIENTS:
                queryClients(client, outputStream);
                break;
            case ClientOpcodes.QUERY_CLIENT_RESOURCES:
                try (XLock lock = xServer.lock(XServer.Lockable.PIXMAP_MANAGER)) {
                    queryClientResources(client, inputStream, outputStream);
                }
                break;
            case ClientOpcodes.QUERY_CLIENT_PIXMAP_BYTES:
                try (XLock lock = xServer.lock(XServer.Lockable.PIXMAP_MANAGER)) {
                    queryClientPixmapBytes(client, inputStream, outputStream);
                }
                break;
            case ClientOpcodes.QUERY_CLIENT_IDS:
            case ClientOpcodes.QUERY_RESOURCE_BYTES:
                emptyList(client, inputStream, outputStream);
                break;
            default:
                throw new BadImplementation();
        }
    }
}
