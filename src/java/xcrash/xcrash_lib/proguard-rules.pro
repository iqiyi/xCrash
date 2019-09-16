-keep class xcrash.NativeHandler {
    native <methods>;
    void crashCallback(...);
    void traceCallback(...);
}
