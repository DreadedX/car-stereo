package dev.huizinga.receiverstats

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.ParcelUuid
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.*
import androidx.compose.material.AlertDialog
import androidx.compose.material.Button
import androidx.compose.material.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.google.accompanist.permissions.ExperimentalPermissionsApi
import com.google.accompanist.permissions.PermissionStatus
import com.google.accompanist.permissions.rememberPermissionState
import dev.huizinga.receiverstats.ui.theme.ReceiverStatsTheme

private const val TAG = "MainActivity"

@ExperimentalPermissionsApi
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            ReceiverStatsTheme {
                // A surface container using the 'background' color from the theme
//                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colors.background) {
//                    TestButton()
//                }

                ScanButton(isScanning, startScan = { startBleScan() }, stopScan = { stopBleScan() })
            }
        }
    }

    private val bluetoothAdapter: BluetoothAdapter by lazy {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothManager.adapter
    }

    private val bleScanner by lazy {
        bluetoothAdapter.bluetoothLeScanner
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            with (result.device) {
                Log.i(TAG, "Found BLE device! Name: ${name ?: "Unnamed"}, address: $address")
            }
        }
    }

    private val scanSettings = ScanSettings.Builder()
//        .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
        .build()

    private val scanFilter = ScanFilter.Builder()
        .setServiceUuid(ParcelUuid.fromString("0000180F-0000-1000-8000-00805F9B34FB"))
        .build()

    private val isScanning = mutableStateOf(false)
    private fun startBleScan() {
        bleScanner.startScan(listOf(scanFilter), scanSettings, scanCallback)
        isScanning.value = true
    }

    private fun stopBleScan() {
        bleScanner.stopScan(scanCallback)
        isScanning.value = false
    }

    // @TODO Handle this properly as this requires BLUETOOTH_CONNECT on Android 12
    private fun promptEnableBluetooth() {
        if (!bluetoothAdapter.isEnabled) {
            val intent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            startActivity(intent)
        }
    }

    override fun onResume() {
        super.onResume()
        if (!bluetoothAdapter.isEnabled) {
            promptEnableBluetooth()
        }
    }
}

@Composable
@ExperimentalPermissionsApi
fun ScanButton(
    isScanning: MutableState<Boolean>,
    startScan: () -> Unit,
    stopScan: () -> Unit
) {
    val permissionState = rememberPermissionState(if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) { Manifest.permission.BLUETOOTH_SCAN } else { Manifest.permission.ACCESS_FINE_LOCATION})
    var openDialog by remember { mutableStateOf(false) }

    if (isScanning.value) {
        Button(onClick = stopScan) {
            Text("Stop scan")
        }

    } else {
        Button(onClick = {
            when (permissionState.status) {
                is PermissionStatus.Granted -> {
                    Log.i(TAG, "Starting BLE scan")
                    startScan()
                }
                is PermissionStatus.Denied -> {
                    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
                        openDialog = true
                    } else {
                        permissionState.launchPermissionRequest()
                    }
                }
            }
        }) {
            Text("Scan for device")
        }
    }

    // This dialog is here to explain why we need location permission
    if (openDialog) {
        AlertDialog(
            onDismissRequest = { openDialog = false},
            title = {
                Text("Location permission required")
            },
            text = {
                Text("In order to scan for BLE devices the location permission is required.")
            },
            confirmButton = {
                Button(onClick = {
                    permissionState.launchPermissionRequest()
                    openDialog = false
                }) {
                    Text("Understood")
                }
            }
        )
    }
}

@Composable
fun TestButton(modifier: Modifier = Modifier) {
    Column(modifier = modifier.padding(16.dp)) {
        var clicks by rememberSaveable { mutableStateOf(0) }

        if (clicks > 0) {
            Text("You have clicked the button $clicks times!")
        }

        val enabled = clicks < 10;
        Button(onClick = { clicks++ }, Modifier.padding(top = 8.dp), enabled) {
            if (enabled) {
                Text("Click me")
            } else {
                Text("Enough")
            }

        }
    }
}

@Preview
@Composable
fun TestButtonPreview() {
    TestButton()
}

@Composable
fun BatteryVoltage(voltage: Float) {
    Column {
        Text("Battery Level")
        Text(voltage.toString())
    }
}

@Preview
@Composable
fun BatteryVoltagePreview() {
    BatteryVoltage(voltage = 9.2f)
}
