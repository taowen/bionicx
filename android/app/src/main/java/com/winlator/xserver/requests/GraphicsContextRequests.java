package com.winlator.xserver.requests;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xserver.Drawable;
import com.winlator.xserver.GraphicsContext;
import com.winlator.core.Bitmask;
import com.winlator.xserver.XClient;
import com.winlator.xserver.errors.BadDrawable;
import com.winlator.xserver.errors.BadGraphicsContext;
import com.winlator.xserver.errors.BadIdChoice;
import com.winlator.xserver.errors.XRequestError;

import java.util.ArrayList;

public abstract class GraphicsContextRequests {
    public static void createGC(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int gcId = inputStream.readInt();
        int drawableId = inputStream.readInt();
        Bitmask valueMask = new Bitmask(inputStream.readInt());

        if (!client.isValidResourceId(gcId)) throw new BadIdChoice(gcId);

        Drawable drawable = client.xServer.drawableManager.getDrawable(drawableId);
        if (drawable == null) throw new BadDrawable(drawableId);
        GraphicsContext graphicsContext = client.xServer.graphicsContextManager.createGraphicsContext(gcId, drawable);
        if (graphicsContext == null) throw new BadIdChoice(gcId);

        client.registerAsOwnerOfResource(graphicsContext);
        if (!valueMask.isEmpty()) client.xServer.graphicsContextManager.updateGraphicsContext(graphicsContext, valueMask, inputStream);
    }

    public static void changeGC(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        int gcId = inputStream.readInt();
        Bitmask valueMask = new Bitmask(inputStream.readInt());
        GraphicsContext graphicsContext = client.xServer.graphicsContextManager.getGraphicsContext(gcId);
        if (graphicsContext == null) throw new BadGraphicsContext(gcId);

        if (!valueMask.isEmpty()) client.xServer.graphicsContextManager.updateGraphicsContext(graphicsContext, valueMask, inputStream);
    }

    public static void freeGC(XClient client, XInputStream inputStream, XOutputStream outputStream) throws XRequestError {
        client.xServer.graphicsContextManager.freeGraphicsContext(inputStream.readInt());
    }

    public static void setClipRectangles(XClient client,
            XInputStream inputStream, XOutputStream outputStream)
            throws XRequestError {
        int graphicsContextId = inputStream.readInt();
        GraphicsContext graphicsContext = client.xServer.graphicsContextManager
                .getGraphicsContext(graphicsContextId);
        if (graphicsContext == null)
            throw new BadGraphicsContext(graphicsContextId);
        graphicsContext.setClipXOrigin(inputStream.readShort());
        graphicsContext.setClipYOrigin(inputStream.readShort());
        int remaining = client.getRemainingRequestLength();
        if ((remaining & 7) != 0) throw new BadValue(remaining);
        ArrayList<GraphicsContext.ClipRectangle> rectangles = new ArrayList<>();
        while (remaining >= 8) {
            rectangles.add(new GraphicsContext.ClipRectangle(
                    inputStream.readShort(), inputStream.readShort(),
                    inputStream.readUnsignedShort(),
                    inputStream.readUnsignedShort()));
            remaining -= 8;
        }
        graphicsContext.setClipRectangles(rectangles);
    }
}
