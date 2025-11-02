import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:geolocator/geolocator.dart';
import 'package:http/http.dart' as http;

void main() => runApp(const PetTrackerApp());

class PetTrackerApp extends StatelessWidget {
  const PetTrackerApp({super.key});
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: "Pet Tracker",
      theme: ThemeData(primarySwatch: Colors.blue),
      home: TrackerHome(),
    );
  }
}

class TrackerHome extends StatefulWidget {
  @override
  _TrackerHomeState createState() => _TrackerHomeState();
}

class _TrackerHomeState extends State<TrackerHome> {
  BluetoothDevice? device;
  BluetoothCharacteristic? notifyChar;
  BluetoothCharacteristic? locReqChar;
  LatLng? petLocation;
  LatLng? phoneLocation;
  bool isConnected = false;
  bool canRequestLocation = false;
  bool isWaiting = false;
  StreamSubscription<List<int>>? notifSub;

  List<LatLng> routePoints = [];

  final String serviceUuid = "de8a5aac-a99b-c315-0c80-60d4cbb51224";
  final String notifyUuid = "61a885a4-41c3-60d0-9a53-6d652a70d29c";
  final String locReqUuid = "5b026510-4088-c297-46d8-be6c736a087a";

  @override
  void initState() {
    super.initState();
    // updatePhoneLocation();
    scanAndConnect();
  }

  Future<void> updatePhoneLocation() async {
    LocationPermission permission = await Geolocator.checkPermission();
    if (permission == LocationPermission.denied) {
      permission = await Geolocator.requestPermission();
      if (permission == LocationPermission.denied || permission == LocationPermission.deniedForever) return;
    }
    if (permission == LocationPermission.deniedForever) return;
    Position pos = await Geolocator.getCurrentPosition();
    setState(() {
      phoneLocation = LatLng(pos.latitude, pos.longitude);
    });
  }

  void scanAndConnect() async {
    FlutterBluePlus.startScan(timeout: const Duration(seconds: 5));
    FlutterBluePlus.scanResults.listen((results) async {
      for (var r in results) {
        if (r.device.name == "XIAO_MG24 Server") {
          FlutterBluePlus.stopScan();
          device = r.device;
          await device!.connect();
          setState(() => isConnected = true);

          var services = await device!.discoverServices();
          for (var s in services) {
            if (s.uuid.toString() == serviceUuid) {
              for (var c in s.characteristics) {
                if (c.uuid.toString() == notifyUuid) {
                  notifyChar = c;
                }
                if (c.uuid.toString() == locReqUuid) {
                  locReqChar = c;
                }
              }
            }
          }

          if (notifyChar != null) {
            await notifyChar!.setNotifyValue(true);
            notifSub = notifyChar!.value.listen((value) {
              final data = utf8.decode(value);
              final parts = data.split(',');
              if (parts.length == 2) {
                double lat = double.tryParse(parts[0].trim()) ?? 0;
                double lon = double.tryParse(parts[1].trim()) ?? 0;
                setState(() {
                  petLocation = LatLng(lat, lon);
                  isWaiting = false;
                  canRequestLocation = true;
                });
                fetchRoute(); // Fetch snapped route after new pet location
              }
            });
          }
          setState(() {
            canRequestLocation = locReqChar != null;
          });
          break;
        }
      }
    });
  }

  Future<void> fetchRoute() async {
    if (phoneLocation == null || petLocation == null) {
      setState(() {
        routePoints = [];
      });
      return;
    }
    final start = "${phoneLocation!.longitude},${phoneLocation!.latitude}";
    final end = "${petLocation!.longitude},${petLocation!.latitude}";
    final url = Uri.parse(
        "http://router.project-osrm.org/route/v1/foot/$start;$end?overview=full&geometries=geojson");
    try {
      final response = await http.get(url);
      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        final coords = data['routes'][0]['geometry']['coordinates'] as List;
        setState(() {
          routePoints = coords
              .map((c) => LatLng(c[1] as double, c[0] as double))
              .toList();
        });
      } else {
        setState(() {
          routePoints = [];
        });
      }
    } catch (e) {
      setState(() {
        routePoints = [];
      });
    }
  }

  Future<void> requestLocation() async {
    await updatePhoneLocation();
    if (locReqChar != null) {
      setState(() {
        isWaiting = true;
        canRequestLocation = false;
      });
      await locReqChar!.write([0x01], withoutResponse: false);
      Future.delayed(const Duration(seconds: 10)).then((_) {
        if (mounted && isWaiting) {
          setState(() {
            isWaiting = false;
            canRequestLocation = true;
          });
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text("No location response received.")),
          );
        }
      });
      fetchRoute(); // Calculate route on each request
    }
  }

  @override
  void dispose() {
    notifSub?.cancel();
    device?.disconnect();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    LatLng? mapCenter;
    if (petLocation != null && phoneLocation != null) {
      mapCenter = LatLng(
        (petLocation!.latitude + phoneLocation!.latitude) / 2,
        (petLocation!.longitude + phoneLocation!.longitude) / 2,
      );
    } else {
      mapCenter = petLocation ?? phoneLocation;
    }

    return Scaffold(
      appBar: AppBar(title: const Text("Pet Tracker")),
      body: Center(
        child: !isConnected
            ? ElevatedButton(
                child: const Text("Connect to MG24"),
                onPressed: scanAndConnect,
              )
            : Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  if (petLocation != null || phoneLocation != null)
                    SizedBox(
                      height: 300,
                      child: FlutterMap(
                        options: MapOptions(
                          center: mapCenter,
                          zoom: 16,
                        ),
                        children: [
                          TileLayer(
                            urlTemplate:
                              "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
                            subdomains: const ['a', 'b', 'c'],
                          ),
                          MarkerLayer(
                            markers: [
                              if (petLocation != null)
                                Marker(
                                  point: petLocation!,
                                  width: 50,
                                  height: 50,
                                  child: const Icon(Icons.pets, color: Colors.red, size: 40),
                                ),
                              if (phoneLocation != null)
                                Marker(
                                  point: phoneLocation!,
                                  width: 50,
                                  height: 50,
                                  child: const Icon(Icons.my_location, color: Colors.blue, size: 40),
                                ),
                            ],
                          ),
                          PolylineLayer(
                            polylines: [
                              if (routePoints.isNotEmpty)
                                Polyline(
                                  points: routePoints,
                                  strokeWidth: 4,
                                  color: Colors.green,
                                ),
                            ],
                          ),
                        ],
                      ),
                    ),
                  const SizedBox(height: 24),
                  ElevatedButton(
                    onPressed: isWaiting || !canRequestLocation ? null : requestLocation,
                    child: isWaiting
                        ? const SizedBox(
                            width: 20,
                            height: 20,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Text("TRACK PET"),
                  ),
                  const SizedBox(height: 24),
                  if (petLocation != null)
                    Text("Pet: ${petLocation!.latitude}, ${petLocation!.longitude}"),
                  if (phoneLocation != null)
                    Text("You: ${phoneLocation!.latitude}, ${phoneLocation!.longitude}"),
                ],
              ),
      ),
    );
  }
}
