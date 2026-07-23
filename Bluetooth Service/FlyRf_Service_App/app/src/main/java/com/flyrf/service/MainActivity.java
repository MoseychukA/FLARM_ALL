package com.flyrf.service;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.method.ScrollingMovementMethod;
import android.view.Gravity;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.UUID;

public class MainActivity extends Activity {
    private static final UUID UART_SERVICE = UUID.fromString("0000ffe0-0000-1000-8000-00805f9b34fb");
    private static final UUID UART_CHAR = UUID.fromString("0000ffe1-0000-1000-8000-00805f9b34fb");
    private static final UUID UART_RX_CHAR = UUID.fromString("0000ffe2-0000-1000-8000-00805f9b34fb");
    private static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private static final int REQ_PERMS = 10;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final LinkedHashMap<String, BluetoothDevice> devices = new LinkedHashMap<>();
    private final ArrayList<String> deviceLabels = new ArrayList<>();

    private TextView status;
    private TextView log;
    private ArrayAdapter<String> adapter;
    private BluetoothAdapter bluetoothAdapter;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic uartChar;
    private BluetoothGattCharacteristic rxChar;
    private boolean scanning;
    private boolean connectFirstFound;
    private boolean bleReady;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        BluetoothManager manager = (BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
        bluetoothAdapter = manager != null ? manager.getAdapter() : null;
        scanner = bluetoothAdapter != null ? bluetoothAdapter.getBluetoothLeScanner() : null;
        buildUi();
        ensurePermissions();
    }

    private void buildUi() {
        if (Build.VERSION.SDK_INT >= 21) {
            getWindow().setStatusBarColor(Color.rgb(238, 242, 247));
        }

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(28, statusBarHeight() + 20, 28, 28);
        root.setBackgroundColor(Color.rgb(248, 250, 252));

        status = new TextView(this);
        status.setText("Disconnected");
        status.setTextSize(18);
        status.setTextColor(Color.rgb(31, 41, 55));
        status.setPadding(0, 0, 0, 8);
        root.addView(status, new LinearLayout.LayoutParams(-1, -2));

        Button scan = button("Scan", Color.rgb(207, 226, 243));
        scan.setOnClickListener(v -> startScan());
        root.addView(scan, lp(-1, 176, 0, 0, 0, 14));

        ListView list = new ListView(this);
        adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, deviceLabels);
        list.setAdapter(adapter);
        list.setDividerHeight(1);
        list.setBackgroundColor(Color.TRANSPARENT);
        list.setOnItemClickListener((parent, view, position, id) -> connect(deviceLabels.get(position)));
        root.addView(list, new LinearLayout.LayoutParams(-1, 0, 1));

        LinearLayout commands = new LinearLayout(this);
        commands.setOrientation(LinearLayout.VERTICAL);
        commands.addView(commandButton("WiFi OFF", "WIFI OFF\n", Color.rgb(245, 214, 214)), lp(-1, 176, 0, 0, 0, 14));
        commands.addView(commandButton("WiFi ON", "WIFI ON\n", Color.rgb(211, 237, 218)), lp(-1, 176, 0, 0, 0, 14));
        commands.addView(commandButton("Status", "STATUS\n", Color.rgb(207, 226, 243)), lp(-1, 176, 0, 0, 0, 14));
        commands.addView(commandButton("Restart", "RESTART\n", Color.rgb(224, 226, 230)), lp(-1, 176, 0, 0, 0, 16));
        root.addView(commands, new LinearLayout.LayoutParams(-1, -2));

        log = new TextView(this);
        log.setTextSize(16);
        log.setTextColor(Color.rgb(95, 99, 104));
        log.setGravity(Gravity.BOTTOM);
        log.setMovementMethod(new ScrollingMovementMethod());
        root.addView(log, new LinearLayout.LayoutParams(-1, 160));

        setContentView(root);
    }

    private Button button(String text, int color) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextColor(Color.rgb(31, 41, 55));
        b.setTextSize(18);
        b.setGravity(Gravity.CENTER);
        b.setMinHeight(0);
        b.setMinimumHeight(0);
        b.setPadding(0, 0, 0, 0);
        b.setBackground(round(color, 8));
        return b;
    }

    private Button commandButton(String label, String command, int color) {
        Button b = button(label, color);
        b.setOnClickListener(v -> send(command));
        return b;
    }

    private LinearLayout row(Button left, Button right) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.addView(left, lp(0, 52, 1f, 0, 0, 6));
        row.addView(right, lp(0, 52, 1f, 6, 0, 0));
        row.setPadding(0, 0, 0, 8);
        return row;
    }

    private LinearLayout.LayoutParams lp(int w, int h, int l, int t, int r, int b) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(w, h);
        p.setMargins(l, t, r, b);
        return p;
    }

    private LinearLayout.LayoutParams lp(int w, int h, float weight, int l, int r, int b) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(w, h, weight);
        p.setMargins(l, 0, r, b);
        return p;
    }

    private GradientDrawable round(int color, int radius) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(radius);
        return d;
    }

    private int statusBarHeight() {
        int resId = getResources().getIdentifier("status_bar_height", "dimen", "android");
        if (resId > 0) {
            return getResources().getDimensionPixelSize(resId);
        }
        return 36;
    }

    private void ensurePermissions() {
        ArrayList<String> needed = new ArrayList<>();
        if (Build.VERSION.SDK_INT >= 31) {
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.BLUETOOTH_SCAN);
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) needed.add(Manifest.permission.BLUETOOTH_CONNECT);
        } else if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_FINE_LOCATION);
        }
        if (!needed.isEmpty()) requestPermissions(needed.toArray(new String[0]), REQ_PERMS);
    }

    private boolean hasBlePermission() {
        if (Build.VERSION.SDK_INT >= 31) {
            return checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                   checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED;
        }
        return checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }

    private void startScan() {
        if (!hasBlePermission()) {
            ensurePermissions();
            return;
        }
        if (scanner == null || scanning) return;
        devices.clear();
        deviceLabels.clear();
        adapter.notifyDataSetChanged();
        scanning = true;
        connectFirstFound = true;
        status.setText("Scanning...");
        scanner.startScan(scanCallback);
        handler.postDelayed(this::stopScan, 10000);
    }

    private void stopScan() {
        if (!scanning || scanner == null || !hasBlePermission()) return;
        scanner.stopScan(scanCallback);
        scanning = false;
        status.setText(devices.isEmpty() ? "No FlyRF devices found" : "Select device");
    }

    private final ScanCallback scanCallback = new ScanCallback() {
        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            BluetoothDevice d = result.getDevice();
            if (d == null || !hasBlePermission()) return;
            ScanRecord record = result.getScanRecord();
            String name = record != null ? record.getDeviceName() : null;
            if (name == null || name.isEmpty()) name = d.getName();
            if (!isFlyRfDevice(name)) return;
            String label = name + "\n" + d.getAddress();
            runOnUiThread(() -> {
                if (!devices.containsKey(label)) {
                    devices.put(label, d);
                    deviceLabels.add(label);
                    adapter.notifyDataSetChanged();
                    if (connectFirstFound) {
                        connectFirstFound = false;
                        connect(label);
                    }
                }
            });
        }
    };

    private boolean isFlyRfDevice(String name) {
        if (name == null) return false;
        String normalized = name.toLowerCase(java.util.Locale.ROOT);
        return normalized.startsWith("flyrf_base-") || normalized.startsWith("flyrf_disp-");
    }

    private void connect(String label) {
        if (!hasBlePermission()) return;
        stopScan();
        BluetoothDevice d = devices.get(label);
        if (d == null) return;
        if (gatt != null) gatt.close();
        uartChar = null;
        rxChar = null;
        bleReady = false;
        status.setText("Connecting " + label.split("\n")[0]);
        if (Build.VERSION.SDK_INT >= 23) {
            gatt = d.connectGatt(this, false, gattCallback, BluetoothDevice.TRANSPORT_LE);
        } else {
            gatt = d.connectGatt(this, false, gattCallback);
        }
    }

    private final BluetoothGattCallback gattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt g, int statusCode, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED && statusCode == BluetoothGatt.GATT_SUCCESS && hasBlePermission()) {
                runOnUiThread(() -> status.setText("Connected, refreshing BLE"));
                refreshGattCache(g);
                handler.postDelayed(() -> {
                    if (hasBlePermission()) {
                        runOnUiThread(() -> status.setText("Discovering services"));
                        g.requestMtu(185);
                        g.discoverServices();
                    }
                }, 700);
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                uartChar = null;
                rxChar = null;
                bleReady = false;
                runOnUiThread(() -> status.setText(statusCode == BluetoothGatt.GATT_SUCCESS ? "Disconnected" : "Connection failed " + statusCode));
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt g, int statusCode) {
            if (statusCode != BluetoothGatt.GATT_SUCCESS) {
                runOnUiThread(() -> status.setText("Service scan failed " + statusCode));
                return;
            }
            BluetoothGattService s = g.getService(UART_SERVICE);
            uartChar = s != null ? s.getCharacteristic(UART_CHAR) : null;
            rxChar = s != null ? s.getCharacteristic(UART_RX_CHAR) : null;
            if (rxChar == null) rxChar = uartChar;
            if (uartChar != null && hasBlePermission()) {
                runOnUiThread(() -> appendLog("RX " + shortUuid(rxChar) + " props " + rxChar.getProperties() + "\n"));
                g.setCharacteristicNotification(uartChar, true);
                BluetoothGattDescriptor desc = uartChar.getDescriptor(CCCD);
                if (desc != null) {
                    desc.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                    bleReady = false;
                    runOnUiThread(() -> status.setText("Preparing UART"));
                    if (!g.writeDescriptor(desc)) {
                        bleReady = true;
                        runOnUiThread(() -> status.setText("Ready"));
                    }
                } else {
                    bleReady = true;
                    runOnUiThread(() -> status.setText("Ready"));
                }
            } else {
                bleReady = false;
                runOnUiThread(() -> status.setText("UART service not found"));
            }
        }

        @Override
        public void onDescriptorWrite(BluetoothGatt g, BluetoothGattDescriptor descriptor, int statusCode) {
            if (CCCD.equals(descriptor.getUuid())) {
                bleReady = statusCode == BluetoothGatt.GATT_SUCCESS;
                runOnUiThread(() -> status.setText(bleReady ? "Ready" : "UART notify failed " + statusCode));
            }
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt g, BluetoothGattCharacteristic c) {
            if (UART_CHAR.equals(c.getUuid())) {
                String text = new String(c.getValue(), StandardCharsets.UTF_8);
                runOnUiThread(() -> appendLog(text));
            }
        }
    };

    private void send(String command) {
        if (gatt == null || rxChar == null || !bleReady || !hasBlePermission()) {
            appendLog("Not connected\n");
            return;
        }
        int props = rxChar.getProperties();
        byte[] data = command.getBytes(StandardCharsets.UTF_8);
        int firstType = (props & BluetoothGattCharacteristic.PROPERTY_WRITE) != 0
                ? BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                : BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE;
        int secondType = firstType == BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                ? BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
                : BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT;
        boolean ok = writeCommand(data, firstType);
        if (!ok) ok = writeCommand(data, secondType);
        appendLog("> " + command + (ok ? "" : "write failed\n"));
    }

    private boolean writeCommand(byte[] data, int writeType) {
        rxChar.setWriteType(writeType);
        if (Build.VERSION.SDK_INT >= 33) {
            return gatt.writeCharacteristic(rxChar, data, writeType) == 0;
        }
        rxChar.setValue(data);
        return gatt.writeCharacteristic(rxChar);
    }

    private void refreshGattCache(BluetoothGatt g) {
        try {
            Method m = g.getClass().getMethod("refresh");
            m.invoke(g);
        } catch (Exception ignored) {
        }
    }

    private String shortUuid(BluetoothGattCharacteristic c) {
        if (c == null) return "none";
        String u = c.getUuid().toString();
        return u.length() >= 8 ? u.substring(4, 8).toUpperCase() : u;
    }

    private void appendLog(String text) {
        log.append(text);
        int scroll = log.getLayout() == null ? 0 : log.getLayout().getLineTop(log.getLineCount()) - log.getHeight();
        if (scroll > 0) log.scrollTo(0, scroll);
    }

    @Override
    protected void onDestroy() {
        stopScan();
        if (gatt != null && hasBlePermission()) {
            gatt.disconnect();
            gatt.close();
        }
        super.onDestroy();
    }
}
