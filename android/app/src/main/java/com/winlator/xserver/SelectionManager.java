package com.winlator.xserver;

import android.util.SparseArray;

import com.winlator.xserver.events.SelectionClear;
import com.winlator.xserver.events.SelectionNotify;
import com.winlator.xserver.events.SelectionRequest;

import java.util.ArrayList;

public class SelectionManager implements XResourceManager.OnResourceLifecycleListener {
    public static final int REASON_SET_OWNER = 0;
    public static final int REASON_WINDOW_DESTROY = 1;
    public static final int REASON_CLIENT_CLOSE = 2;

    private final SparseArray<Selection> selections = new SparseArray<>();
    private final ArrayList<OnSelectionModificationListener> listeners = new ArrayList<>();

    public interface OnSelectionModificationListener {
        void onSelectionModified(int atom, Window owner, int timestamp,
                                 int reason, XClient ownerClient);
    }

    public SelectionManager(WindowManager windowManager) {
        windowManager.addOnResourceLifecycleListener(this);
    }

    public static class Selection {
        public Window owner;
        private XClient client;
        private int timestamp;
    }

    public void setSelection(int atom, Window owner, XClient client, int timestamp) {
        Selection selection = getSelection(atom);
        if (selection.owner != null && selection.owner != owner) {
            selection.client.sendEvent(new SelectionClear(
                    timestamp, selection.owner, atom));
        }
        selection.owner = owner;
        selection.client = owner != null ? client : null;
        selection.timestamp = timestamp;
        for (int i = listeners.size() - 1; i >= 0; i--) {
            listeners.get(i).onSelectionModified(atom, owner, timestamp,
                    REASON_SET_OWNER, client);
        }
    }

    public void convertSelection(XClient requestorClient, Window requestor,
                                 int selectionAtom, int target, int property,
                                 int timestamp) {
        Selection selection = getSelection(selectionAtom);
        if (selection.owner != null && selection.client != null) {
            selection.client.sendEvent(new SelectionRequest(timestamp,
                    selection.owner.id, requestor.id, selectionAtom,
                    target, property));
        }
        else {
            requestorClient.sendEvent(new SelectionNotify(timestamp,
                    requestor.id, selectionAtom, target, 0));
        }
    }

    public void addOnSelectionModificationListener(OnSelectionModificationListener listener) {
        if (!listeners.contains(listener)) listeners.add(listener);
    }

    public Selection getSelection(int atom) {
        Selection selection = selections.get(atom);
        if (selection != null) return selection;
        selection = new Selection();
        selections.put(atom, selection);
        return selection;
    }

    public void releaseClientSelections(XClient client) {
        for (int i = 0; i < selections.size(); i++) {
            int atom = selections.keyAt(i);
            Selection selection = selections.valueAt(i);
            if (selection.client == client && selection.owner != null) {
                notifyLifecycle(atom, selection, REASON_CLIENT_CLOSE);
                selection.owner = null;
                selection.client = null;
            }
        }
    }

    private void notifyLifecycle(int atom, Selection selection, int reason) {
        for (int i = listeners.size() - 1; i >= 0; i--) {
            listeners.get(i).onSelectionModified(atom, selection.owner,
                    selection.timestamp, reason, selection.client);
        }
    }

    @Override
    public void onFreeResource(XResource resource) {
        for (int i = 0; i < selections.size(); i++) {
            int atom = selections.keyAt(i);
            Selection selection = selections.valueAt(i);
            if (selection.owner == resource) {
                notifyLifecycle(atom, selection, REASON_WINDOW_DESTROY);
                selection.owner = null;
                selection.client = null;
            }
        }
    }
}
