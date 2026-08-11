package com.winlator.xserver;

import com.winlator.xconnector.ConnectedClient;
import com.winlator.xconnector.ConnectionHandler;

public class XClientConnectionHandler implements ConnectionHandler {
    private final XServer xServer;

    public XClientConnectionHandler(XServer xServer) {
        this.xServer = xServer;
    }

    @Override
    public ConnectedClient newConnectedClient(long clientPtr, int fd) {
        return new XClient(clientPtr, fd, xServer);
    }

    @Override
    public void handleNewConnection(ConnectedClient client) {
        xServer.addClient((XClient)client);
    }

    @Override
    public void handleConnectionShutdown(ConnectedClient client) {
        XClient xClient = (XClient)client;
        xServer.selectionManager.releaseClientSelections(xClient);
        xClient.freeResources();
        xServer.removeClient(xClient);
    }
}
