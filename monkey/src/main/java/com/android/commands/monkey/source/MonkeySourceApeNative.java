/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */

package com.android.commands.monkey.source;

import static com.android.commands.monkey.fastbot.client.ActionType.SCROLL_BOTTOM_UP;
import static com.android.commands.monkey.fastbot.client.ActionType.SCROLL_TOP_DOWN;
import static com.android.commands.monkey.framework.AndroidDevice.stopPackage;
import static com.android.commands.monkey.utils.Config.bytestStatusBarHeight;
import static com.android.commands.monkey.utils.Config.defaultGUIThrottle;
import static com.android.commands.monkey.utils.Config.doHistoryRestart;
import static com.android.commands.monkey.utils.Config.doHoming;
import static com.android.commands.monkey.utils.Config.execPreShell;
import static com.android.commands.monkey.utils.Config.execPreShellEveryStartup;
import static com.android.commands.monkey.utils.Config.execSchema;
import static com.android.commands.monkey.utils.Config.execSchemaEveryStartup;
import static com.android.commands.monkey.utils.Config.fuzzingRate;
import static com.android.commands.monkey.utils.Config.historyRestartRate;
import static com.android.commands.monkey.utils.Config.homeAfterNSecondsofsleep;
import static com.android.commands.monkey.utils.Config.homingRate;
import static com.android.commands.monkey.utils.Config.imageWriterCount;
import static com.android.commands.monkey.utils.Config.refectchInfoCount;
import static com.android.commands.monkey.utils.Config.refectchInfoWaitingInterval;
import static com.android.commands.monkey.utils.Config.saveGUITreeToXmlEveryStep;
import static com.android.commands.monkey.utils.Config.schemaTraversalMode;
import static com.android.commands.monkey.utils.Config.scrollAfterNSecondsofsleep;
import static com.android.commands.monkey.utils.Config.startAfterDoScrollAction;
import static com.android.commands.monkey.utils.Config.startAfterDoScrollActionTimes;
import static com.android.commands.monkey.utils.Config.startAfterDoScrollBottomAction;
import static com.android.commands.monkey.utils.Config.startAfterDoScrollBottomActionTimes;
import static com.android.commands.monkey.utils.Config.startAfterNSecondsofsleep;
import static com.android.commands.monkey.utils.Config.swipeDuration;
import static com.android.commands.monkey.utils.Config.takeScreenshotForEveryStep;
import static com.android.commands.monkey.utils.Config.throttleForExecPreSchema;
import static com.android.commands.monkey.utils.Config.throttleForExecPreShell;
import static com.android.commands.monkey.utils.Config.useRandomClick;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.IActivityManager;
import android.app.UiAutomation;
import android.app.UiAutomationConnection;
import android.content.ComponentName;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.PointF;
import android.graphics.Rect;
import android.hardware.display.DisplayManagerGlobal;
import android.os.Build;
import android.os.HandlerThread;

import java.lang.Exception;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.IWindowManager;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.accessibility.AccessibilityNodeInfo;

import android.util.Base64;
import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.List;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import com.android.commands.monkey.Monkey;
import com.android.commands.monkey.utils.MonkeyUtils;
import com.android.commands.monkey.action.Action;
import com.android.commands.monkey.action.FuzzAction;
import com.android.commands.monkey.action.ModelAction;
import com.android.commands.monkey.events.MonkeyEvent;
import com.android.commands.monkey.events.MonkeyEventQueue;
import com.android.commands.monkey.events.MonkeyEventSource;
import com.android.commands.monkey.events.base.MonkeyActivityEvent;
import com.android.commands.monkey.events.base.MonkeyCommandEvent;
import com.android.commands.monkey.events.base.MonkeyIMEEvent;
import com.android.commands.monkey.events.base.MonkeyKeyEvent;
import com.android.commands.monkey.events.base.MonkeyDataActivityEvent;
import com.android.commands.monkey.events.base.MonkeyRotationEvent;
import com.android.commands.monkey.events.base.MonkeySchemaEvent;
import com.android.commands.monkey.events.base.MonkeyThrottleEvent;
import com.android.commands.monkey.events.base.MonkeyTouchEvent;
import com.android.commands.monkey.events.base.MonkeyWaitEvent;
import com.android.commands.monkey.events.customize.ClickEvent;
import com.android.commands.monkey.events.CustomEvent;
import com.android.commands.monkey.events.CustomEventFuzzer;
import com.android.commands.monkey.events.customize.ShellEvent;
import com.android.commands.monkey.fastbot.client.ActionType;
import com.android.commands.monkey.fastbot.client.Operate;
import com.android.commands.monkey.framework.AndroidDevice;
import com.android.commands.monkey.events.base.mutation.MutationAirplaneEvent;
import com.android.commands.monkey.events.base.mutation.MutationAlwaysFinishActivityEvent;
import com.android.commands.monkey.events.base.mutation.MutationWifiEvent;
import com.android.commands.monkey.provider.SchemaProvider;
import com.android.commands.monkey.provider.ShellProvider;
import com.android.commands.monkey.tree.TreeBuilder;
import com.android.commands.monkey.utils.Config;
import com.android.commands.monkey.utils.ImageWriterQueue;
import com.android.commands.monkey.utils.Logger;
import com.android.commands.monkey.utils.RandomHelper;
import com.android.commands.monkey.utils.UUIDHelper;
import com.android.commands.monkey.utils.Utils;
import com.bytedance.fastbot.AiClient;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Random;
import java.util.Stack;
import java.util.concurrent.TimeoutException;


/**
 * @author Zhao Zhang
 */

/**
 * Monkey event core class, get gui page information, jni call native layer to generate events, inject execution events
 */
public class MonkeySourceApeNative implements MonkeyEventSource {

    private static long CLICK_WAIT_TIME = 0L;
    private static long LONG_CLICK_WAIT_TIME = 1000L;
    /**
     * UiAutomation client and connection
     */
    protected UiAutomation mUiAutomation;
    protected final HandlerThread mHandlerThread = new HandlerThread("MonkeySourceApeNative");
    public Monkey monkey = null;

    private int timestamp = 0;
    private int lastInputTimestamp = -1;

    private List<ComponentName> mMainApps;
    private Map<String, String[]> packagePermissions;
    /**
     * total number of events generated so far
     */
    private int mEventCount = 0;
    /**
     * monkey event queue
     */
    private MonkeyEventQueue mQ;
    /**
     * debug level
     */
    private int mVerbose = 0;
    /**
     * The delay between event inputs
     **/
    private long mThrottle = defaultGUIThrottle;
    /**
     * Whether to randomize each throttle (0-mThrottle ms) inserted between
     * events.
     */
    private boolean mRandomizeThrottle = false;
    /**
     * random generator
     */
    private Random mRandom;

    private int mEventId = 0;
    /**
     * customize the height of the top tarbar of the device, this area needs to be cropped out
     */
    private int statusBarHeight = bytestStatusBarHeight;

    private File mOutputDirectory;
    /**
     * screenshot asynchronous storage queue
     */
    private ImageWriterQueue[] mImageWriters;
    /**
     * Record tested activities, but there are activities that may miss quick jumps
     */
    private HashSet<String> activityHistory = new HashSet<>();
    private String currentActivity = "";
    /**
     * appliaction total、stub、plugin activity
     */
    private HashSet<String> mTotalActivities = new HashSet<>();
    private HashSet<String> stubActivities = new HashSet<>();
    private HashSet<String> pluginActivities = new HashSet<>();

    private static Locale stringFormatLocale = Locale.ENGLISH;

    private int timeStep = 0;
    /**
     * deviceid from /sdcard/max.uuid, If read null, generate a random one locally
     */
    private String did = UUIDHelper.read();
    /**
     * execute shell only on first startup
     */
    private boolean firstExecShell = true;
    /**
     * Execute schema only on first startup
     */
    private boolean firstSchema = true;
    /**
     * Record executed schemas for schema traversal
     */
    private Stack<String> schemaStack = new Stack<>();

    private String appVersion = "";
    private String packageName = "";

    /**
     * user-defined application launcher activity intent
     */
    private String intentAction = null;
    /**
     * user-defined application launcher activity intent data
     */
    private String intentData = null;
    /**
     * user-defined application launcher activity, not used for now
     */
    private String quickActivity = null;

    private int appRestarted = 0;
    private boolean fullFuzzing = true;


    public MonkeySourceApeNative(Random random, List<ComponentName> MainApps,
                                 long throttle, boolean randomizeThrottle, boolean permissionTargetSystem,
                                 File outputDirectory) {

        mRandom = random;
        mMainApps = MainApps;
        mThrottle = throttle;
        mRandomizeThrottle = randomizeThrottle;
        mQ = new MonkeyEventQueue(random, 0, false); // we manage throttle
        mOutputDirectory = outputDirectory;

        packagePermissions = new HashMap<>();
        for (ComponentName app : MainApps) {
            packagePermissions.put(app.getPackageName(), AndroidDevice.getGrantedPermissions(app.getPackageName()));
        }
        mImageWriters = new ImageWriterQueue[imageWriterCount];
        for (int i = 0; i < 3; i++) {
            mImageWriters[i] = new ImageWriterQueue();
            Thread imageThread = new Thread(mImageWriters[i]);
            imageThread.start();
        }
        getTotalActivities();
        connect();

        Logger.println("// device uuid is " + did);
    }

    /**
     * In mathematics, linear interpolation is a method of curve fitting using linear polynomials
     * to construct new data points within the range of a discrete set of known data points.
     * @param a
     * @param b
     * @param alpha
     * @return
     */
    private static float lerp(float a, float b, float alpha) {
        return (b - a) * alpha + a;
    }

    /**
     * Connect to AccessibilityService
     */
    public void connect() {
        if (mHandlerThread.isAlive()) {
            throw new IllegalStateException("Already connected!");
        }
        mHandlerThread.start();
        mUiAutomation = new UiAutomation(mHandlerThread.getLooper(), new UiAutomationConnection());
        mUiAutomation.connect();

        AccessibilityServiceInfo info = mUiAutomation.getServiceInfo();
        // Compress this node
        info.flags &= ~AccessibilityServiceInfo.FLAG_INCLUDE_NOT_IMPORTANT_VIEWS;

        mUiAutomation.setServiceInfo(info);
    }

    /**
     * Disconnect to AccessibilityService
     */
    public void disconnect() {
        if (!mHandlerThread.isAlive()) {
            throw new IllegalStateException("Already disconnected!");
        }
        if (mUiAutomation != null) {
            mUiAutomation.disconnect();
        }
        mHandlerThread.quit();
    }

    public int getEventCount() {
        return mEventCount;
    }

    public void tearDown() {
        this.disconnect();
        this.printCoverage();
        for (ImageWriterQueue writer : mImageWriters) {
            writer.tearDown();
        }

        // 在测试结束时清理 native 资源并保存模型数据
        Logger.println("// MonkeySourceApeNative tearDown - cleaning up native resources");
        AiClient.cleanupAndSaveModel();
    }

    public boolean validate() {
        return mHandlerThread.isAlive();
    }

    public void setVerbose(int verbose) {
        mVerbose = verbose;
    }

    public int getStatusBarHeight() {
        if (this.statusBarHeight == 0) {
            Display display = DisplayManagerGlobal.getInstance().getRealDisplay(Display.DEFAULT_DISPLAY);
            DisplayMetrics dm = new DisplayMetrics();
            display.getMetrics(dm);
            int w = display.getWidth();
            int h = display.getHeight();
            if (w == 1080 && h > 2100) {
                statusBarHeight = (int) (40 * dm.density);
            } else if (w == 1200 && h == 1824) {
                statusBarHeight = (int) (30 * dm.density);
            } else if (w == 1440 && h == 2696) {
                statusBarHeight = (int) (30 * dm.density);
            } else {
                statusBarHeight = (int) (24 * dm.density);
            }
        }
        return this.statusBarHeight;
    }

    /**
     * Init and loading reuse model
     */
    public void initReuseAgent() {
        AiClient.InitAgent(AiClient.AlgorithmType.Reuse, this.packageName);
    }

    /**
     * ActiveWindow may not belong to activity package.
     *
     * @return AccessibilityNodeInfo of the root
     */
    public AccessibilityNodeInfo getRootInActiveWindow() {
        return mUiAutomation.getRootInActiveWindow();
    }

    public AccessibilityNodeInfo getRootInActiveWindowSlow() {
        try {
            mUiAutomation.waitForIdle(1000, 1000 * 10);
        } catch (TimeoutException e) {
            //e.printStackTrace();
        }
        return mUiAutomation.getRootInActiveWindow();
    }

    private final boolean hasEvent() {
        return !mQ.isEmpty();
    }

    private final void addEvent(MonkeyEvent event) {
        mQ.addLast(event);
        event.setEventId(mEventId++);
    }

    private final void addEvents(List<MonkeyEvent> events){
        for (int i = 0; i < events.size(); i++) {
            addEvent(events.get(i));
        }
    }

    private final void clearEvent() {
        while (!mQ.isEmpty()) {
            MonkeyEvent e = mQ.removeFirst();
        }
    }

    /**
     * generate an activity event
     */
    private final MonkeyEvent popEvent() {
        return mQ.removeFirst();
    }

    void resetRotation() {
        addEvent(new MonkeyRotationEvent(Surface.ROTATION_0, false));
    }

    void sleep(long time) {
        try {
            Thread.sleep(time);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    /**
     * if the queue is empty, we generate events first
     *
     * @return the first event in the queue
     */
    public MonkeyEvent getNextEvent() {
        checkAppActivity();
        if (!hasEvent()) {
            try {
                // 在生成事件前获取当前UI状态
                AccessibilityNodeInfo rootNode = getRootInActiveWindow();
                ComponentName topActivityName = this.getTopActivityComponentName();
                
                if (rootNode != null && topActivityName != null) {
                    // 提取当前界面上所有可交互元素的图标
                    try {
                        extractWidgetIcons(rootNode, topActivityName.getClassName());
                    } catch (Exception e) {
                        Logger.errorPrintln("Error extracting widget icons: " + e.getMessage());
                    }
                }
                
                generateEvents();
            } catch (RuntimeException e) {
                Logger.errorPrintln(e.getMessage());
                e.printStackTrace();
                clearEvent();
                return null;
            }
        }
        mEventCount++;
        return popEvent();
    }

    // public MonkeyEvent getNextEvent() {
    //     checkAppActivity();
    //     if (!hasEvent()) {
    //         try {
    //             // 在生成事件前获取当前UI状态
    //             AccessibilityNodeInfo rootNode = getRootInActiveWindow();
    //             if (rootNode != null) {// 应添加判断当前页面是否访问过的功能-todo
    //                 // 提取当前界面上所有可交互元素的图标
    //                 extractWidgetIcons(rootNode);
    //                 rootNode.recycle(); // 确保回收资源
    //             }
                
    //             generateEvents();
    //         } catch (RuntimeException e) {
    //             Logger.errorPrintln(e.getMessage());
    //             e.printStackTrace();
    //             clearEvent();
    //             return null;
    //         }
    //     }
    //     mEventCount++;
    //     return popEvent();
    // }

    /**
     * 提取当前界面上所有可交互元素的图标
     * @param rootNode 当前界面的根节点
     * @param activityName 当前活动名称
     */
    private void extractWidgetIcons(AccessibilityNodeInfo rootNode, String activityName) {
        if (rootNode == null || activityName == null || activityName.isEmpty()) {
            Logger.errorPrintln("Invalid parameters for extractWidgetIcons");
            return;
        }
        
        try {
            // 创建一个Map存储所有提取的图标信息
        Map<String, String> iconMap = new HashMap<>();
        
            // 递归遍历UI树并提取图标，限制最大图标数量以避免内存问题
        extractWidgetIconsRecursive(rootNode, iconMap);
        
        // 如果有图标被提取，将它们序列化并传递到C++端
        if (!iconMap.isEmpty()) {
                Logger.println("Extracted " + iconMap.size() + " widget icons from " + activityName);
            String serializedIcons = serializeWidgetIcons(iconMap);
                
            // 调用JNI方法将序列化后的图标信息传递到C++端
                try {
                    sendWidgetIconsToNative(activityName, serializedIcons);
                } catch (Exception e) {
                    Logger.errorPrintln("Failed to send widget icons to native: " + e.getMessage());
                }
            }
        } catch (Exception e) {
            Logger.errorPrintln("Error in extractWidgetIcons: " + e.getMessage());
        }
    }

    private void extractWidgetIconsRecursive(AccessibilityNodeInfo node, Map<String, String> iconMap) {
        if (node == null) return;
        
        // 检查节点是否可点击、可滚动等可交互属性
        if (node.isClickable() || node.isScrollable() || node.isLongClickable()) {
            // 获取节点的边界
            Rect bounds = new Rect();
            node.getBoundsInScreen(bounds);
            
            // 捕获该区域的截图作为图标
            Bitmap iconBitmap = null;
            try {
                iconBitmap = captureWidgetIcon(bounds);
                if (iconBitmap != null && !iconBitmap.isRecycled()) {
                // 生成widget的唯一标识
                String widgetId = getWidgetIdentifier(node);
                
                // 将图标转换为Base64字符串
                String base64Icon = bitmapToBase64(iconBitmap);
                
                    // 只有当转换成功时才添加到Map中
                    if (!base64Icon.isEmpty()) {
                iconMap.put(widgetId, base64Icon);
                    }
                }
            } catch (Exception e) {
                Logger.errorPrintln("Error processing widget icon: " + e.getMessage());
            } finally {
                // 确保在使用完毕后安全回收Bitmap
                if (iconBitmap != null && !iconBitmap.isRecycled()) {
                    iconBitmap.recycle();
                }
            }
        }
        
        // 递归处理子节点
        for (int i = 0; i < node.getChildCount(); i++) {
            AccessibilityNodeInfo childNode = node.getChild(i);
            if (childNode != null) {
                try {
                extractWidgetIconsRecursive(childNode, iconMap);
                } finally {
                childNode.recycle(); // 确保回收子节点资源
                }
            }
        }
    }
    
    /**
     * 获取Widget的唯一标识符
     * @param node AccessibilityNodeInfo节点
     * @return Widget的唯一标识符
     */
    private String getWidgetIdentifier(AccessibilityNodeInfo node) {
        // 优先使用resourceId作为标识符
        String resourceId = node.getViewIdResourceName();
        if (resourceId != null && !resourceId.isEmpty()) {
            return resourceId;
        }
        
        // 如果没有resourceId，使用类名和边界作为标识
        StringBuilder identifier = new StringBuilder();
        if (node.getClassName() != null) {
            identifier.append(node.getClassName());
        }
        
        Rect bounds = new Rect();
        node.getBoundsInScreen(bounds);
        identifier.append("_").append(bounds.toShortString());
        
        // 如果有文本或内容描述，也加入到标识中
        if (node.getText() != null) {
            identifier.append("_").append(node.getText());
        } else if (node.getContentDescription() != null) {
            identifier.append("_").append(node.getContentDescription());
        }
        
        return identifier.toString();
    }
    /**
     * 捕获指定区域的截图作为Widget图标
     * @param bounds 要捕获的区域
     * @return 捕获的Bitmap，如果失败则返回null
     */
    private Bitmap captureWidgetIcon(Rect bounds) {
        Bitmap screenBitmap = null;
        try {
            // 捕获整个屏幕
            screenBitmap = mUiAutomation.takeScreenshot();
            if (screenBitmap == null) {
                Logger.errorPrintln("Failed to take screenshot");
                return null;
            }

            // 确保边界在屏幕范围内
            int screenWidth = screenBitmap.getWidth();
            int screenHeight = screenBitmap.getHeight();

            if (bounds.left < 0 || bounds.top < 0 || 
                bounds.right > screenWidth || bounds.bottom > screenHeight ||
                bounds.width() <= 0 || bounds.height() <= 0) {
                Logger.errorPrintln("Invalid bounds: " + bounds.toShortString());
                return null;
            }

            // 裁剪出Widget区域
            Bitmap iconBitmap = Bitmap.createBitmap(
                screenBitmap, 
                bounds.left, 
                bounds.top, 
                bounds.width(), 
                bounds.height()
            );

            return iconBitmap;
        } catch (Exception e) {
            Logger.errorPrintln("Failed to capture widget icon: " + e.getMessage());
            return null;
        } finally {
            // 回收屏幕截图资源
            if (screenBitmap != null && !screenBitmap.isRecycled()) {
                screenBitmap.recycle();
            }
        }
    }

    /**
     * 将Bitmap转换为字节数组
     * @param bitmap 要转换的Bitmap
     * @return 转换后的字节数组
     */ 
    private String bitmapToBase64(Bitmap bitmap) {
        if (bitmap == null || bitmap.isRecycled()) {
            Logger.errorPrintln("Cannot convert null or recycled bitmap to Base64");
            return "";
        }
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        try {
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);
        byte[] byteArray = stream.toByteArray();
        return Base64.encodeToString(byteArray, Base64.DEFAULT);
        } catch (Exception e) {
            Logger.errorPrintln("Error converting bitmap to Base64: " + e.getMessage());
            return "";
        } finally {
            try {
                stream.close();
            } catch (Exception e) {
                // 忽略关闭流的异常
            }
        }
    }
    /**
     * 序列化Widget图标信息为JSON字符串
     * @param iconMap 包含widget ID和对应图标Base64字符串的映射
     * @return 序列化后的JSON字符串
     */
    private String serializeWidgetIcons(Map<String, String> iconMap) {
        if (iconMap == null || iconMap.isEmpty()) {
            return "{}";
        }
        
        try {
            JSONObject jsonObject = new JSONObject();
            
            // 限制图标数量，避免JSON过大
            int maxIcons = 20;
            int count = 0;

            // 遍历Map，将每个键值对添加到JSON对象中
            for (Map.Entry<String, String> entry : iconMap.entrySet()) {
                if (count++ >= maxIcons) break;
                
                String key = entry.getKey();
                String value = entry.getValue();
                
                // 验证值不为空且长度合理
                if (key != null && value != null && !value.isEmpty() && value.length() < 100000) {
                    jsonObject.put(key, value);
                }
            }

            return jsonObject.toString();
        } catch (JSONException e) {
            Logger.errorPrintln("Failed to serialize widget icons: " + e.getMessage());
            return "{}";
        } catch (OutOfMemoryError e) {
            Logger.errorPrintln("Out of memory when serializing widget icons");
            return "{}";
        }
    }

    
    /**
     * 将序列化后的Widget图标信息发送到C++端
     * @param activityName 当前活动的名称
     * @param serializedIcons 序列化后的图标信息
     */
    private void sendWidgetIconsToNative(String activityName, String serializedIcons) {
        try {
            AiClient.setWidgetIcons(activityName, serializedIcons);
        } catch (Exception e) {
            Logger.errorPrintln("Failed to send widget icons to native: " + e.getMessage());
        }
    }

    // /**
    //  * 存储Widget图标信息的类
    //  */
    // private static class WidgetIconInfo {
    //     public String id;          // Widget的资源ID
    //     public Rect bounds;        // Widget在屏幕上的边界
    //     public String className;   // Widget的类名
    //     public String text;        // Widget的文本内容
    //     public String contentDesc; // Widget的内容描述
    //     public byte[] iconData;    // Widget图标的图像数据
    //     public boolean isClickable;      // 是否可点击
    //     public boolean isScrollable;     // 是否可滚动
    //     public boolean isLongClickable;  // 是否可长按
    // }

    public Random getRandom() {
        return mRandom;
    }

    public long getThrottle() {
        return this.mThrottle;
    }

    /**
     * Get the top Activity info from the Activity stack
     * @return Component name of the top activity
     */
    private ComponentName getTopActivityComponentName() {
        return AndroidDevice.getTopActivityComponentName();
    }

    /**
     * If the given component is not allowed to interact with, start a random app or
     * generating a fuzzing action
     * @param cn Component that is not allowed to interact with
     */
    private void dealWithBlockedActivity(ComponentName cn) {
        String className = cn.getClassName();
        if (!hasEvent()) {
            if (appRestarted == 0) {
                Logger.println("// the top activity is " + className + ", not testing app, need inject restart app");
                startRandomMainApp();
                appRestarted = 1;
            } else {
                if (!AndroidDevice.isAtPhoneLauncher(className)) {
                    Logger.println("// the top activity is " + className + ", not testing app, need inject fuzz event");
                    Action fuzzingAction = generateFuzzingAction(true);
                    generateEventsForAction(fuzzingAction);
                } else {
                    fullFuzzing = false;
                }
                appRestarted = 0;
            }
        }
    }

    /**
     * If this activity could be interacted with. Should be in white list or not in blacklist or
     * not specified.
     * @param cn Component Name of this activity
     * @return If could be interacted, return true
     */
    private boolean checkAppActivity(ComponentName cn) {
        return cn == null || MonkeyUtils.getPackageFilter().checkEnteringPackage(cn.getPackageName());
    }

    protected void checkAppActivity() {
        ComponentName cn = getTopActivityComponentName();
        if (cn == null) {
            Logger.println("// get activity api error");
            clearEvent();
            startRandomMainApp();
            return;
        }
        String className = cn.getClassName();
        String pkg = cn.getPackageName();
        boolean allow = MonkeyUtils.getPackageFilter().checkEnteringPackage(pkg);

        if (allow) {
            if (!this.currentActivity.equals(className)) {
                this.currentActivity = className;
                activityHistory.add(this.currentActivity);
                Logger.println("// current activity is " + this.currentActivity);
                timestamp++;
            }
        }else
            dealWithBlockedActivity(cn);
    }

    /**
     * Calling this method, you could delete the user data and revoke granted permission of
     * this specific package.
     * @param packageName The package name of which data to delete.
     */
    public void clearPackage(String packageName) {
        String[] permissions = this.packagePermissions.get(packageName);
        if (permissions == null) {
            Logger.warningPrintln("Stop clearing untracked package: " + packageName);
            return;
        }
        if(AndroidDevice.clearPackage(packageName, permissions))
            Logger.infoPrintln("Package "+packageName+" cleared.");
    }

    /**
     * Generate events for activity
     * @param app The info about this activity.
     * @param clearPackage If should delete the user data and revoke granted permissions
     * @param startFromHistory If need to start activity form history stack
     */
    protected void generateActivityEvents(ComponentName app, boolean clearPackage, boolean startFromHistory) {
        if (clearPackage) {
            clearPackage(app.getPackageName());
        }
        generateShellEvents();
        boolean startbyHistory = false; // if should start activity from history stack
        if (startFromHistory && doHistoryRestart && RandomHelper.toss(historyRestartRate)) {
            Logger.println("start from history task");
            startbyHistory = true;
        }
        if (intentData != null) { // if not null, start activity with intent and the data inside
            MonkeyDataActivityEvent e = new MonkeyDataActivityEvent(app, intentAction, intentData, quickActivity, startbyHistory);
            addEvent(e);
        } else { // default
            MonkeyActivityEvent e = new MonkeyActivityEvent(app, startbyHistory);
            addEvent(e);
        }
        generateThrottleEvent(startAfterNSecondsofsleep); // waiting for the loading of apps
        generateSchemaEvents();
        generateActivityScrollEvents();
    }

    /**
     * Generate scrolling events
     */
    private void generateActivityScrollEvents() {
        if (startAfterDoScrollAction) {
            int i = startAfterDoScrollActionTimes;
            while (i-- > 0) {
                generateScrollEventAt(AndroidDevice.getDisplayBounds(), SCROLL_TOP_DOWN);
                generateThrottleEvent(scrollAfterNSecondsofsleep);
            }
        }

        if (startAfterDoScrollBottomAction) {
            int i = startAfterDoScrollBottomActionTimes;
            while (i-- > 0) {
                generateScrollEventAt(AndroidDevice.getDisplayBounds(), SCROLL_BOTTOM_UP);
                generateThrottleEvent(scrollAfterNSecondsofsleep);
            }
        }
    }

    /**
     * Restart the specific package
     * @param cn Component Name of the specific app activity
     * @param clearPackage If should clear user data and permissions or not
     * @param reason String reason to restart package
     */
    protected void restartPackage(ComponentName cn, boolean clearPackage, String reason) {
        if (doHoming && RandomHelper.toss(homingRate)) {
            Logger.println("press HOME before app kill");
            generateKeyEvent(KeyEvent.KEYCODE_HOME);
            generateThrottleEvent(homeAfterNSecondsofsleep);
        }
        String packageName = cn.getPackageName();
        Logger.infoPrintln("Try to restart package " + packageName + " for " + reason);
        stopPackage(cn.getPackageName());
        generateActivityEvents(cn, clearPackage, true);
    }

    /**
     * Pick an activity that we can interact with.
     * @return Chosen activity component name
     */
    public ComponentName randomlyPickMainApp() {
        int total = mMainApps.size();
        int index = mRandom.nextInt(total);
        return mMainApps.get(index);
    }

    protected void startRandomMainApp() {
        generateActivityEvents(randomlyPickMainApp(), false, false);
    }

    protected void generateThrottleEvent(long base) {
        long throttle = base;
        if (mRandomizeThrottle && (throttle > 0)) {
            throttle = mRandom.nextLong();
            if (throttle < 0) {
                throttle = -throttle;
            }
            throttle %= base;
            ++throttle;
        }
        if (throttle < 0) {
            throttle = -throttle;
        }
        addEvent(new MonkeyThrottleEvent(throttle));
    }

    protected void generateActivateEvent() { // duplicated with custmozie
        Logger.infoPrintln("generate app switch events.");
        generateAppSwitchEvent();
    }

    private void generateAppSwitchEvent() {
        generateKeyEvent(KeyEvent.KEYCODE_APP_SWITCH);
        generateThrottleEvent(500);
        if (RandomHelper.nextBoolean()) {
            Logger.println("press HOME after app switch");
            generateKeyEvent(KeyEvent.KEYCODE_HOME);
        } else {
            Logger.println("press BACK after app switch");
            generateKeyEvent(KeyEvent.KEYCODE_BACK);
        }
        generateThrottleEvent(mThrottle);
    }

    protected void generateKeyEvent(int key) {
        MonkeyKeyEvent e = new MonkeyKeyEvent(KeyEvent.ACTION_DOWN, key);
        addEvent(e);

        e = new MonkeyKeyEvent(KeyEvent.ACTION_UP, key);
        addEvent(e);
    }

    private void attemptToSendTextByKeyEvents(String inputText) {
        char[] szRes = inputText.toCharArray(); // Convert String to Char array

        KeyCharacterMap CharMap;
        if (Build.VERSION.SDK_INT >= 11) // My soft runs until API 5
            CharMap = KeyCharacterMap.load(KeyCharacterMap.VIRTUAL_KEYBOARD);
        else
            CharMap = KeyCharacterMap.load(KeyCharacterMap.ALPHA);

        KeyEvent[] events = CharMap.getEvents(szRes);

        for (int i = 0; i < events.length; i += 2) {
            generateKeyEvent(events[i].getKeyCode());
        }
        generateKeyEvent(KeyEvent.KEYCODE_ENTER);
    }

    private PointF shieldBlackRect(PointF p) {
        // move to native: AiClient.checkPointIsShield
        int retryTimes = 10;
        PointF p1 = p;
        do {
            if (!AiClient.checkPointIsShield(this.currentActivity, p1)) {
                break;
            }
            // re generate a point
            Rect displayBounds = AndroidDevice.getDisplayBounds();
            float unitx = displayBounds.height() / 20.0f;
            float unity = displayBounds.width() / 10.0f;
            p1.x = p.x + retryTimes * unitx * RandomHelper.nextInt(8);
            p1.y = p.y + retryTimes * unity * RandomHelper.nextInt(17);
            p1.x = p1.x % displayBounds.width();
            p1.y = p1.y % displayBounds.height();
        } while (retryTimes-- > 0);
        return p1;
    }

    protected void generateClickEventAt(Rect nodeRect, long waitTime) {
        generateClickEventAt(nodeRect, waitTime, useRandomClick);
    }

    /**
     * Generate click event at the given rectangle area
     * @param nodeRect the given rectangle area to click
     * @param waitTime after performing click, the time to wait for
     * @param useRandomClick if should perform click randomly in rectangle area
     */
    protected void generateClickEventAt(Rect nodeRect, long waitTime, boolean useRandomClick) {
        Rect bounds = nodeRect;
        if (bounds == null) {
            Logger.warningPrintln("Error to fetch bounds.");
            bounds = AndroidDevice.getDisplayBounds();
        }

        PointF p1;
        if (useRandomClick) {
            int width = bounds.width() > 0 ? getRandom().nextInt(bounds.width()) : 0;
            int height = bounds.height() > 0 ? getRandom().nextInt(bounds.height()) : 0;
            p1 = new PointF(bounds.left + width, bounds.top + height);
        } else
            p1 = new PointF(bounds.left + bounds.width()/2.0f, bounds.top + bounds.height()/2.0f);
        if (!bounds.contains((int) p1.x, (int) p1.y)) {
            Logger.warningFormat("Invalid bounds: %s", bounds);
            return;
        }
        p1 = shieldBlackRect(p1);
        long downAt = SystemClock.uptimeMillis();

        addEvent(new MonkeyTouchEvent(MotionEvent.ACTION_DOWN).setDownTime(downAt).addPointer(0, p1.x, p1.y)
                .setIntermediateNote(false));

        if (waitTime > 0) {
            MonkeyWaitEvent we = new MonkeyWaitEvent(waitTime);
            addEvent(we);
        }

        addEvent(new MonkeyTouchEvent(MotionEvent.ACTION_UP).setDownTime(downAt).addPointer(0, p1.x, p1.y)
                .setIntermediateNote(false));
    }

    private void generateScrollEventAt(Rect nodeRect, ActionType type) {
        Rect displayBounds = AndroidDevice.getDisplayBounds();
        if (nodeRect == null) {
            nodeRect = AndroidDevice.getDisplayBounds();
        }

        PointF start = new PointF(nodeRect.exactCenterX(), nodeRect.exactCenterY());
        PointF end;

        switch (type) {
            case SCROLL_BOTTOM_UP:
                int top = getStatusBarHeight();
                if (top < displayBounds.top) {
                    top = displayBounds.top;
                }
                end = new PointF(start.x, top); // top is inclusive
                break;
            case SCROLL_TOP_DOWN:
                end = new PointF(start.x, displayBounds.bottom - 1); // bottom is
                // exclusive
                break;
            case SCROLL_LEFT_RIGHT:
                end = new PointF(displayBounds.right - 1, start.y); // right is
                // exclusive
                break;
            case SCROLL_RIGHT_LEFT:
                end = new PointF(displayBounds.left, start.y); // left is inclusive
                break;
            default:
                throw new RuntimeException("Should not reach here");
        }

        long downAt = SystemClock.uptimeMillis();


        addEvent(new MonkeyTouchEvent(MotionEvent.ACTION_DOWN).setDownTime(downAt).addPointer(0, start.x, start.y)
                .setIntermediateNote(false).setType(1));

        int steps = 10;
        long waitTime = swipeDuration / steps;
        for (int i = 0; i < steps; i++) {
            float alpha = i / (float) steps;
            addEvent(new MonkeyTouchEvent(MotionEvent.ACTION_MOVE).setDownTime(downAt)
                    .addPointer(0, lerp(start.x, end.x, alpha), lerp(start.y, end.y, alpha)).setIntermediateNote(true).setType(1));
            addEvent(new MonkeyWaitEvent(waitTime));
        }

        addEvent(new MonkeyTouchEvent(MotionEvent.ACTION_UP).setDownTime(downAt).addPointer(0, end.x, end.y)
                .setIntermediateNote(false).setType(1));
    }

    /**
     * According to the returned action from native model, parse the text inside, and
     * input those texts if IME is activated.
     * @param action returned action from native model
     */
    private void doInput(ModelAction action) {
        String inputText = action.getInputText();
        boolean useAdbInput = action.isUseAdbInput();
        if (inputText != null && !inputText.equals("")) {
            Logger.println("Input text is " + inputText);
            if (action.isClearText())
                generateClearEvent(action.getBoundingBox());

            if (action.isRawInput()) {
                if (!AndroidDevice.sendText(inputText))
                    attemptToSendTextByKeyEvents(inputText);
                return;
            }

            if (!useAdbInput) {
                Logger.println("MonkeyIMEEvent added " + inputText);
                addEvent(new MonkeyIMEEvent(inputText));
            } else {
                Logger.println("MonkeyCommandEvent added " + inputText);
                addEvent(new MonkeyCommandEvent("input text " + inputText));
            }

        } else {
            if (lastInputTimestamp == timestamp) {
                Logger.warningPrintln("checkVirtualKeyboard: Input only once.");
                return;
            } else {
                lastInputTimestamp = timestamp;
            }
            if (action.isEditText() || AndroidDevice.isVirtualKeyboardOpened()) {
                generateKeyEvent(KeyEvent.KEYCODE_ESCAPE);
            }
        }
    }

    private void generateClearEvent(Rect bounds) {
        generateClickEventAt(bounds, LONG_CLICK_WAIT_TIME);
        generateKeyEvent(KeyEvent.KEYCODE_DEL);
        generateClickEventAt(bounds, CLICK_WAIT_TIME);
    }

    public Bitmap captureBitmap() {
        return mUiAutomation.takeScreenshot();
    }

    public File getOutputDir() {
        return mOutputDirectory;
    }


    /**
     * If current window belongs to the system UI, randomly pick an allowed app to start
     * @param info AccessibilityNodeInfo of the root of this current window
     * @return If this window belongs to system UI, return true
     */
    public boolean dealWithSystemUI(AccessibilityNodeInfo info)
    {
        if(info == null || info.getPackageName() == null)
        {
            Logger.println("get null accessibility node");
            return false;
        }
        String packageName = info.getPackageName().toString();
        if(packageName.equals("com.android.systemui")) {

            Logger.println("get notification window or other system windows");
            Rect bounds = AndroidDevice.getDisplayBounds();
            // press home
            generateKeyEvent(KeyEvent.KEYCODE_HOME);
            //scroll up
            generateScrollEventAt(bounds, SCROLL_BOTTOM_UP);
            // launch app
            generateActivityEvents(randomlyPickMainApp(), false, false);
            generateThrottleEvent(1000);
            return true;
        }
        return false;
    }


    /**
     * generate a random event based on mFactor
     */
    protected void generateEvents() {
        long start = System.currentTimeMillis();
        if (hasEvent()) {
            return;
        }

        resetRotation();
        ComponentName topActivityName = null;
        String stringOfGuiTree = "";
        Action fuzzingAction = null;
        AccessibilityNodeInfo info = null;
        int repeat = refectchInfoCount;

        // try to get AccessibilityNodeInfo quickly for several times.
        while (repeat-- > 0) {
            topActivityName = this.getTopActivityComponentName();
            info = getRootInActiveWindow();
            // this two operations may not be the same
            if (info == null || topActivityName == null) {
                sleep(refectchInfoWaitingInterval);
                continue;
            }

            Logger.println("// Event id: " + mEventId);
            if(dealWithSystemUI(info)) {
                // 确保释放资源
                if (info != null) {
                    info.recycle();
                }
                return;
            }
            break;
        }

        // If node is null, try to get AccessibilityNodeInfo slow for only once
        if (info == null) {
            topActivityName = this.getTopActivityComponentName();
            info = getRootInActiveWindowSlow();
            if (info != null) {
                Logger.println("// Event id: " + mEventId);
                if(dealWithSystemUI(info)) {
                    // 确保释放资源
                    info.recycle();
                    return;
                }
            }
        }

        // If node is not null, build tree and recycle this resource.
        if (info != null) {
            try {
            stringOfGuiTree = TreeBuilder.dumpDocumentStrWithOutTree(info);
            if (mVerbose > 3) Logger.println("//" + stringOfGuiTree);
            } catch (Exception e) {
                Logger.errorPrintln("Error dumping GUI tree: " + e.getMessage());
            } finally {
            info.recycle();
            }
        }

        // For user specified actions, during executing, fuzzing is not allowed.
        boolean allowFuzzing = true;

        if (topActivityName != null && !"".equals(stringOfGuiTree)) {
            try {
                long rpc_start = System.currentTimeMillis();

                Operate operate = AiClient.getAction(topActivityName.getClassName(), stringOfGuiTree);
                operate.throttle += (int) this.mThrottle;
                // For user specified actions, during executing, fuzzing is not allowed.
                allowFuzzing = operate.allowFuzzing;
                ActionType type = operate.act;
                Logger.println("action type: " + type.toString());
                Logger.println("rpc cost time: " + (System.currentTimeMillis() - rpc_start));

                Rect rect = new Rect(0, 0, 0, 0);
                List<PointF> pointFloats = new ArrayList<>();

                if (type.requireTarget()) {
                    List<Short> points = operate.pos;
                    if (points != null && points.size() >= 4) {
                        rect = new Rect((Short) points.get(0), (Short) points.get(1), (Short) points.get(2), (Short) points.get(3));
                    } else {
                        type = ActionType.NOP;
                    }
                }

                timeStep++;
                String sid = operate.sid;
                String aid = operate.aid;
                long timeMillis = System.currentTimeMillis();

                if (saveGUITreeToXmlEveryStep) {
                    checkOutputDir();
                    File xmlFile = new File(checkOutputDir(), String.format(stringFormatLocale,
                            "step-%d-%s-%s-%s.xml", timeStep, sid, aid, timeMillis));
                    Logger.infoFormat("Saving GUI tree to %s at step %d %s %s",
                            xmlFile, timeStep, sid, aid);

                    BufferedWriter out = null;
                    try {
                        out = new BufferedWriter(new OutputStreamWriter(new FileOutputStream(xmlFile, false)));
                        out.write(stringOfGuiTree);
                        out.flush();
                    } catch (Exception e) {
                        Logger.errorPrintln("Error saving GUI tree: " + e.getMessage());
                    } finally {
                        if (out != null) {
                            try {
                        out.close();
                            } catch (Exception e) {
                                // 忽略关闭异常
                            }
                        }
                    }
                }

                if (takeScreenshotForEveryStep) {
                    checkOutputDir();
                    File screenshotFile = new File(checkOutputDir(), String.format(stringFormatLocale,
                            "step-%d-%s-%s-%s.png", timeStep, sid, aid, timeMillis));
                    Logger.infoFormat("Saving screen shot to %s at step %d %s %s",
                            screenshotFile, timeStep, sid, aid);
                    takeScreenshot(screenshotFile);
                }

                ModelAction modelAction = new ModelAction(type, topActivityName, pointFloats, rect);
                modelAction.setThrottle(operate.throttle);

                // Complete the info for specific action type
                switch (type) {
                    case CLICK:
                        modelAction.setInputText(operate.text);
                        modelAction.setClearText(operate.clear);
                        modelAction.setEditText(operate.editable);
                        modelAction.setRawInput(operate.rawinput);
                        modelAction.setUseAdbInput(operate.adbinput);
                        break;
                    case LONG_CLICK:
                        modelAction.setWaitTime(operate.waitTime);
                        break;
                    case SHELL_EVENT:
                        modelAction.setShellCommand(operate.text);
                        modelAction.setWaitTime(operate.waitTime);
                        break;
                    default:
                        break;
                }

                generateEventsForAction(modelAction);

                // check if could select next fuzz action from full fuzz-able action options.
                switch (type) {
                    case RESTART:
                    case CLEAN_RESTART:
                    case CRASH:
                        fullFuzzing = false;
                        break;
                    case BACK:
                        fullFuzzing = !AndroidDevice.isAtAppMain(topActivityName.getClassName(), topActivityName.getPackageName());
                        break;
                    default:
                        fullFuzzing = !AndroidDevice.isAtPhoneLauncher(topActivityName.getClassName());
                        break;
                }

            } catch (Exception e) {
                Logger.errorPrintln("Error generating events: " + e.getMessage());
                e.printStackTrace();
                generateThrottleEvent(mThrottle);
            }
        } else {
            Logger.println(
                    "// top activity is null or the corresponding tree is null, " +
                    "accessibility maybe error, fuzz needed."
            );
            fuzzingAction = generateFuzzingAction(fullFuzzing);
            generateEventsForAction(fuzzingAction);
        }

        if (allowFuzzing && fuzzingAction == null && RandomHelper.toss(fuzzingRate)) {
            Logger.println("// generate fuzzing action.");
            fuzzingAction = generateFuzzingAction(fullFuzzing);
            generateEventsForAction(fuzzingAction);
        }

        Logger.println(" event time:" + Long.toString(System.currentTimeMillis() - start));
    }

    private File checkOutputDir() {
        File dir = getOutputDir();
        if (!dir.exists()) {
            dir.mkdirs();
        }
        return dir;
    }

    private ImageWriterQueue nextImageWriter() {
        return mImageWriters[mRandom.nextInt(mImageWriters.length)];
    }

    private void takeScreenshot(File screenshotFile) {
        Bitmap map = mUiAutomation.takeScreenshot();
        nextImageWriter().add(map, screenshotFile);
    }

    /**
     * According to the action type of the action argument, generate its corresponding
     * event, and set throttle if necessary.
     * @param action generated action, could be action from native model, or generated fuzzing
     *               action from CustomEventFuzzer
     */
    private void generateEventsForAction(Action action) {
        generateEventsForActionInternal(action);
        // If this action is for fuzzing, we don't need extra throttle time.
        long throttle = (action instanceof FuzzAction ? 0 : action.getThrottle());
        generateThrottleEvent(throttle);
    }

    /**
     * Return a generated fuzzing action, which could be from complete fuzzing list or simplified
     * fuzzing list
     * @param sampleFromAllFuzzingActions if should select fuzzing action from all possible
     *                                   fuzzing options
     * @return A wrapped action object, containing the generated fuzzing actions.
     */
    private FuzzAction generateFuzzingAction(boolean sampleFromAllFuzzingActions) {
        List<CustomEvent> events = sampleFromAllFuzzingActions ?
                CustomEventFuzzer.generateFuzzingEvents() :
                CustomEventFuzzer.generateSimplifyFuzzingEvents();
        return new FuzzAction(events);
    }

    /**
     * According to the action type of the action argument, generate its corresponding
     * event
     * @param action generated action, could be action from native model, or generated fuzzing
     *               action from CustomEventFuzzer
     */
    private void generateEventsForActionInternal(Action action) {
        ActionType actionType = action.getType();
        switch (actionType) {
            case FUZZ:
                generateFuzzingEvents((FuzzAction) action);
                break;
            case START:
                generateActivityEvents(randomlyPickMainApp(), false, false);
                break;
            case RESTART:
                restartPackage(randomlyPickMainApp(), false, "start action(RESTART)");
                break;
            case CLEAN_RESTART:
                restartPackage(randomlyPickMainApp(), true, "start action(CLEAN_RESTART)");
                break;
            case NOP:
                generateThrottleEvent(action.getThrottle());
                break;
            case ACTIVATE:
                generateActivateEvent();
                break;
            case BACK:
                generateKeyEvent(KeyEvent.KEYCODE_BACK);
                break;
            case CLICK:
                generateClickEventAt(((ModelAction) action).getBoundingBox(), CLICK_WAIT_TIME);
                doInput((ModelAction) action);
                break;
            case LONG_CLICK:
                long waitTime = ((ModelAction) action).getWaitTime();
                if (waitTime == 0) {
                    waitTime = LONG_CLICK_WAIT_TIME;
                }
                generateClickEventAt(((ModelAction) action).getBoundingBox(), waitTime);
                break;
            case SCROLL_BOTTOM_UP:
            case SCROLL_TOP_DOWN:
            case SCROLL_LEFT_RIGHT:
            case SCROLL_RIGHT_LEFT:
                generateScrollEventAt(((ModelAction) action).getBoundingBox(), action.getType());
                break;
            case SCROLL_BOTTOM_UP_N:
                // Scroll from bottom to up for [0,3+5] times.
                int scroll_B_T_N = 3 + RandomHelper.nextInt(5);
                while (scroll_B_T_N-- > 0) {
                    generateScrollEventAt(((ModelAction) action).getBoundingBox(), SCROLL_BOTTOM_UP);
                }
                break;
            case SHELL_EVENT:
                ModelAction modelAction = (ModelAction)action;
                ShellEvent shellEvent = new ShellEvent(modelAction.getShellCommand(), modelAction.getWaitTime());
                List<MonkeyEvent> monkeyEvents = shellEvent.generateMonkeyEvents();
                addEvents(monkeyEvents);
                break;
            default:
                throw new RuntimeException("Should not reach here");
        }
    }

    /**
     * Generate monkey events according to CustomEvents inside FuzzAction
     * @param action Object of FuzzAction, containing all corresponding CustomEvents
     */
    private void generateFuzzingEvents(FuzzAction action) {
        List<CustomEvent> events = action.getFuzzingEvents();
        long throttle = action.getThrottle();
        for (CustomEvent event : events) {
            if (event instanceof ClickEvent) {
                PointF point = ((ClickEvent) event).getPoint();
                point = shieldBlackRect(point);
                ((ClickEvent) event).setPoint(point);
            }
            List<MonkeyEvent> monkeyEvents = event.generateMonkeyEvents();
            for (MonkeyEvent me : monkeyEvents) {
                if (me == null) {
                    throw new RuntimeException();
                }
                addEvent(me);
            }
            generateThrottleEvent(throttle);
        }
    }

    private void getTotalActivities() {
        try {
            for (String p : MonkeyUtils.getPackageFilter().getmValidPackages()) {
                PackageInfo packageInfo = AndroidDevice.packageManager.getPackageInfo(p, PackageManager.GET_ACTIVITIES);
                if (packageInfo != null) {
                    if (packageInfo.packageName.equals("com.android.packageinstaller"))
                        continue;
                    if(packageInfo.activities != null){
                        for (ActivityInfo activityInfo : packageInfo.activities) {
                            mTotalActivities.add(activityInfo.name);
                        }
                    }
                }
            }
        } catch (Exception e) {
        }
    }

    private String getAppVersionCode() {
        try {
            for (String p : MonkeyUtils.getPackageFilter().getmValidPackages()) {
                PackageInfo packageInfo = AndroidDevice.packageManager.getPackageInfo(p, PackageManager.GET_ACTIVITIES);
                if (packageInfo != null) {
                    if (packageInfo.packageName.equals(this.packageName)) {
                        return packageInfo.versionName;
                    }
                }
            }

        } catch (Exception e) {
        }
        return "";
    }

    /**
     * print test activity coverage, cover% = testedActivity/totalActivity
     */
    private void printCoverage() {
        HashSet<String> set = mTotalActivities;

        Logger.println("Total app activities:");
        int i = 0;
        for (String activity : set) {
            i++;
            Logger.println(String.format(Locale.ENGLISH,"%4d %s", i, activity));
        }

        String[] testedActivities = this.activityHistory.toArray(new String[0]);
        Arrays.sort(testedActivities);
        int j = 0;
        String activity = "";
        Logger.println("Explored app activities:");
        for (i = 0; i < testedActivities.length; i++) {
            activity = testedActivities[i];
            if (set.contains(activity)) {
                Logger.println(String.format(Locale.ENGLISH,"%4d %s", j + 1, activity));
                j++;
            }
        }

        float f = 0;
        int s = set.size();
        if (s > 0) {
            f = 1.0f * j / s * 100;
            Logger.println("Activity of Coverage: " + f + "%");
        }

        String[] totalActivities = set.toArray(new String[0]);
        Arrays.sort(totalActivities);
        Utils.activityStatistics(mOutputDirectory, testedActivities, totalActivities, new ArrayList<Map<String, String>>(), f, new HashMap<String, Integer>());
    }

    /**
     * Grant permission to testing app
     * @param packageName package name of the testing app
     * @param reason the reason to grant permission
     */
    public void grantRuntimePermissions(String packageName, String reason) {
        String[] permissions = this.packagePermissions.get(packageName);
        if (permissions == null) {
            Logger.warningPrintln("Stop granting permissions to untracked package: " + packageName);
            return;
        }
        AndroidDevice.grantRuntimePermissions(packageName, permissions, reason);
    }

    /**
     * Grant permission to all testing app
     * @param reason the reason to grant permission
     */
    public void grantRuntimePermissions(String reason) {
        for (ComponentName cn : mMainApps) {
            grantRuntimePermissions(cn.getPackageName(), reason);
        }
    }

    /**
     * Generate mutation event and execute it
     * @param iwm IWindowManager instance
     * @param iam IActivityManager instance
     * @param verbose verbose
     */
    public void startMutation(IWindowManager iwm, IActivityManager iam, int verbose) {
        MonkeyEvent event = null;
        double total = Config.doMutationAirplaneFuzzing + Config.doMutationMutationAlwaysFinishActivitysFuzzing
                + Config.doMutationWifiFuzzing;
        double rate = RandomHelper.nextDouble();
        if (rate < Config.doMutationMutationAlwaysFinishActivitysFuzzing) {
            event = new MutationAlwaysFinishActivityEvent();
        } else if (rate < Config.doMutationMutationAlwaysFinishActivitysFuzzing
                + Config.doMutationWifiFuzzing) {
            event = new MutationWifiEvent();
        } else if (rate < total){
            event = new MutationAirplaneEvent();
        }
        if (event != null) {
            event.injectEvent(iwm, iam, mVerbose);
        }

    }

    /**
     * According to user specified shell commands, choose command randomly
     */
    private void generateShellEvents() {
        if (execPreShell) {
            String command = ShellProvider.randomNext(); // choose command randomly
            if (!"".equals(command) && (firstExecShell || execPreShellEveryStartup)) {
                Logger.println("shell: " + command);
                try {
                    AndroidDevice.executeCommandAndWaitFor(command.split(" "));
                    sleep(throttleForExecPreShell);
                    this.firstExecShell = false;
                } catch (Exception e) {
                }
            }
        }
    }

    /**
     * According to user specified schema, choose schema randomly or in order.
     */
    private void generateSchemaEvents() {
        if (execSchema) {
            if (firstSchema || execSchemaEveryStartup) {
                String schema = SchemaProvider.randomNext(); // choose schema randomly

                if (schemaTraversalMode) { // choose schema in order
                    if (schemaStack.empty()) {
                        ArrayList<String> strings = SchemaProvider.getStrings();
                        for (String s : strings) {
                            schemaStack.push(s);
                        }
                    }
                    if (schemaStack.empty()) return;

                    schema = schemaStack.pop();
                }

                if ("".equals(schema)) return;

                Logger.println("fastbot exec schema: " + schema);
                MonkeySchemaEvent e = new MonkeySchemaEvent(schema);
                addEvent(e);

                generateThrottleEvent(throttleForExecPreSchema);
                this.firstSchema = false;
            }
        }
    }

    /**
     * Set attribute for start user specified intent
     * @param packageName name of package
     * @param appVersion version of app
     * @param intentAction user specified intent
     * @param intentData user specified intent data
     * @param quickActivity user specified activity, not used
     *                      TODO:considering to remove
     */
    public void setAttribute(String packageName, String appVersion, String intentAction, String intentData, String quickActivity) {
        this.packageName = packageName;
        this.appVersion = (!appVersion.equals("")) ? appVersion : this.getAppVersionCode();
        this.intentAction = intentAction;
        this.intentData = intentData;
        this.quickActivity = quickActivity;
    }
}
