package xcrash;

import android.content.Context;
import android.text.TextUtils;
import android.util.Log;
import android.util.Pair;

import java.io.File;

import dalvik.system.PathClassLoader;

public class AbiPathProvider {

    private static final String TAG = "AbiPathProvider";

    public static final String XCRASH_DUMPER_LIB_NAME = "xcrash_dumper";

    public static void test(Context context, String libName) {
        Log.d(TAG, "getAbiPathFromDefault: " + getAbiPathFromDefault(context, libName));
        Log.d(TAG, "getAbiPathFromClassloader: " + getAbiPathFromClassloader(context, libName));
        Log.d(TAG, "getAbiPathFromSplit: " + getAbiPathFromSplit(context));
    }

    public static Pair<String, Boolean> getAbiPath(Context context, String libName) {
        String abiPathFromDefault = getAbiPathFromDefault(context, libName);
        if (!TextUtils.isEmpty(abiPathFromDefault) && isSoExist(abiPathFromDefault, libName)) {
            return new Pair<>(abiPathFromDefault, false);
        }

        String abiPathFromClassloader = getAbiPathFromClassloader(context, libName);
        if (!TextUtils.isEmpty(abiPathFromClassloader)) {
            return new Pair<>(abiPathFromClassloader, true);
        }

        String abiPathFromSplit = getAbiPathFromSplit(context);
        if (!TextUtils.isEmpty(abiPathFromSplit)) {
            return new Pair<>(abiPathFromSplit, true);
        }

        return new Pair<>(context.getApplicationInfo().nativeLibraryDir, false);
    }

    private static String getAbiPathFromDefault(Context context, String libName) {
        return context.getApplicationInfo().nativeLibraryDir;
    }

    private static boolean isSoExist(String nativeLibDir, String libName) {
        File file = new File(nativeLibDir);
        if (file.exists() && file.isDirectory()) {
            File libFile = new File(file, System.mapLibraryName(libName));
            return libFile.exists();
        }
        return false;
    }

    private static String getAbiPathFromClassloader(Context context, String libName) {
        ClassLoader classLoader = context.getClassLoader();

        Log.d(TAG, "classLoader: " + classLoader);

        if (classLoader instanceof PathClassLoader) {
            String libPath = ((PathClassLoader) classLoader).findLibrary(libName);
            if (!TextUtils.isEmpty(libPath)) {
                File parentFile = new File(libPath).getParentFile();
                if (parentFile != null) {
                    return parentFile.getAbsolutePath();
                }
            }
        }

        return "";
    }

    private static String getAbiPathFromSplit(Context context) {
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.LOLLIPOP) {
            return "";
        }

        String[] splitSourceDirs = context.getApplicationInfo().splitSourceDirs;
        AbiName currentAbi = AbiName.ARM64_V8A;
        String path = null;
        for (String splitSourceDir : splitSourceDirs) {
            for (AbiName abiName : AbiName.values()) {
                if (splitSourceDir.contains(abiName.getBundleAbi())) {
                    path = splitSourceDir;
                    break;
                }
            }
            if (path != null) {
                break;
            }
        }

        if (!TextUtils.isEmpty(path)) {
            return path + "!/lib/" + currentAbi.platformName;
        }

        return "";
    }

    enum AbiName {
        ARM64_V8A("arm64-v8a", 64),
        ARMEABI_V7A("armeabi-v7a", 32),
        X86_64("x86_64", 64),
        X86("x86", 32);

        private final String platformName;
        private final int bitSize;
        private final String bundleAbi;

        // Constructor for enum fields
        AbiName(String platformName, int bitSize) {
            this.platformName = platformName;
            this.bitSize = bitSize;
            this.bundleAbi = platformName.replace("-", "_");
        }

        // Getter for platformName
        public String getPlatformName() {
            return platformName;
        }

        // Getter for bitSize
        public int getBitSize() {
            return bitSize;
        }

        // Getter for bundleAbi
        public String getBundleAbi() {
            return bundleAbi;
        }
    }

}
