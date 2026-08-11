package com.winlator.xserver;

import android.util.SparseArray;

import com.winlator.xserver.events.SelectionClear;
import com.winlator.xserver.events.SelectionNotify;
import com.winlator.xserver.events.SelectionRequest;

import java.util.ArrayList;

public class SelectionManager implements XResourceManager.OnResourceLifecycleListener {
    private final SparseArray<Selection> selections = new SparseArray<>();
    private final ArrayList<OnSelectionModificationListener> listeners = new ArrayList<>();

    public interface OnSelectionModificationListener {
        void onSetSelectionOwner(int atom, Window owner, int timestamp);
    }

    public SelectionManager(WindowManager windowManager) {
        windowManager.addOnResourceLifecycleListener(this);
    }

    public static class Selection {
        public Window owner;
        private XClient client;
    }

    public void setSelection(int atom, Window owner, XClient client, int timestamp) {
        Selection selection = getSelection(atom);
        if (selection.owner != null && selection.owner != owner) {
            selection.client.sendEvent(new SelectionClear(
                    timestamp, selection.owner, atom));
        }
        selection.owner = owner;
        selection.client = owner != null ? client : null;
        for (int i = listeners.size() - 1; i >= 0; i--) {
            listeners.get(i).onSetSelectionOwner(atom, owner, timestamp);
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

    @Override
    public void onFreeResource(XResource resource) {
        for (int i = 0; i < selections.size(); i++) {
            Selection selection = selections.valueAt(i);
            if (selection.owner == resource) {
                selection.owner = null;
                selection.client = null;
            }
        }
    }
}
