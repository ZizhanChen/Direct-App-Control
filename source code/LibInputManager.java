import android.annotation.NonNull;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.database.ContentObserver;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayViewport;
import android.hardware.input.IInputDevicesChangedListener;
import android.hardware.input.IInputManager;
import android.hardware.input.ITabletModeChangedListener;
import android.hardware.input.InputDeviceIdentifier;
import android.hardware.input.InputManager;
import android.hardware.input.InputManagerInternal;
import android.hardware.input.KeyboardLayout;
import android.hardware.input.TouchCalibration;
import android.os.Binder;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.os.LocaleList;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.Process;
import android.os.RemoteException;
import android.os.UserHandle;
import android.provider.DeviceConfig;
import android.provider.Settings;
import android.provider.Settings.SettingNotFoundException;
import android.text.TextUtils;
import android.util.Log;
import android.util.Pair;
import android.util.Slog;
import android.util.SparseArray;
import android.view.Display;
import android.view.IInputFilter;
import android.view.IInputFilterHost;
import android.view.IInputMonitorHost;
import android.view.IWindow;
import android.view.InputApplicationHandle;
import android.view.InputChannel;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.InputMonitor;
import android.view.InputWindowHandle;
import android.view.KeyEvent;
import android.view.PointerIcon;
import android.view.Surface;
import android.view.ViewConfiguration;
import android.widget.Toast;

import com.android.internal.R;
import com.android.internal.messages.nano.SystemMessageProto.SystemMessage;
import com.android.internal.notification.SystemNotificationChannels;
import com.android.internal.os.SomeArgs;
import com.android.internal.util.DumpUtils;
import com.android.internal.util.Preconditions;
import com.android.internal.util.XmlUtils;
import com.android.server.DisplayThread;
import com.android.server.LocalServices;
import com.android.server.Watchdog;
import com.android.server.policy.WindowManagerPolicy;

import libcore.io.IoUtils;
import libcore.io.Streams;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Objects;

public class LibInputManager {
    private static native long nativeInit(InputManagerService service,
            Context context, MessageQueue messageQueue);
    private static native void nativeStart(long ptr);
    private static native void nativeSetDisplayViewports(long ptr,
            DisplayViewport[] viewports);

    private static native int nativeGetScanCodeState(long ptr,
            int deviceId, int sourceMask, int scanCode);
    private static native int nativeGetKeyCodeState(long ptr,
            int deviceId, int sourceMask, int keyCode);
    private static native int nativeGetSwitchState(long ptr,
            int deviceId, int sourceMask, int sw);
    private static native boolean nativeHasKeys(long ptr,
            int deviceId, int sourceMask, int[] keyCodes, boolean[] keyExists);
    private static native void nativeRegisterInputChannel(long ptr, InputChannel inputChannel,
            int displayId);
    private static native void nativeRegisterInputMonitor(long ptr, InputChannel inputChannel,
            int displayId, boolean isGestureMonitor);
    private static native void nativeUnregisterInputChannel(long ptr, InputChannel inputChannel);
    private static native void nativePilferPointers(long ptr, IBinder token);
    private static native void nativeSetInputFilterEnabled(long ptr, boolean enable);
    private static native int nativeInjectInputEvent(long ptr, InputEvent event,
            int injectorPid, int injectorUid, int syncMode, int timeoutMillis,
            int policyFlags);
    private static native void nativeToggleCapsLock(long ptr, int deviceId);
    private static native void nativeSetInputWindows(long ptr, InputWindowHandle[] windowHandles,
            int displayId);
    private static native void nativeSetInputDispatchMode(long ptr, boolean enabled, boolean frozen);
    private static native void nativeSetSystemUiVisibility(long ptr, int visibility);
    private static native void nativeSetFocusedApplication(long ptr,
            int displayId, InputApplicationHandle application);
    private static native void nativeSetFocusedDisplay(long ptr, int displayId);
    private static native boolean nativeTransferTouchFocus(long ptr,
            InputChannel fromChannel, InputChannel toChannel);
    private static native void nativeSetPointerSpeed(long ptr, int speed);
    private static native void nativeSetShowTouches(long ptr, boolean enabled);
    private static native void nativeSetInteractive(long ptr, boolean interactive);
    private static native void nativeReloadCalibration(long ptr);
    private static native void nativeVibrate(long ptr, int deviceId, long[] pattern,
            int repeat, int token);
    private static native void nativeCancelVibrate(long ptr, int deviceId, int token);
    private static native void nativeReloadKeyboardLayouts(long ptr);
    private static native void nativeReloadDeviceAliases(long ptr);
    private static native String nativeDump(long ptr);
    private static native void nativeMonitor(long ptr);
    private static native boolean nativeIsInputDeviceEnabled(long ptr, int deviceId);
    private static native void nativeEnableInputDevice(long ptr, int deviceId);
    private static native void nativeDisableInputDevice(long ptr, int deviceId);
    private static native void nativeSetPointerIconType(long ptr, int iconId);
    private static native void nativeReloadPointerIcons(long ptr);
    private static native void nativeSetCustomPointerIcon(long ptr, PointerIcon icon);
    private static native void nativeSetPointerCapture(long ptr, boolean detached);
    private static native boolean nativeCanDispatchToDisplay(long ptr, int deviceId, int displayId);
    private static native void nativedispatchMouse(float x, float y, int w, int h, long ptr);
    private static native void nativedispatchMouseByCd(float x, float y, long ptr);
    private static native void nativeSetMotionClassifierEnabled(long ptr, boolean enabled);
    
    }