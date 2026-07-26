package org.trenchbroom;

import android.view.MotionEvent;

import org.qtproject.qt.android.bindings.QtActivity;

public class TrenchBroomActivity extends QtActivity {
    private static native void nativeMouseButtonReleased();

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        final boolean handled = super.dispatchGenericMotionEvent(event);
        if (event.getActionMasked() == MotionEvent.ACTION_BUTTON_RELEASE) {
            nativeMouseButtonReleased();
        }
        return handled;
    }
}