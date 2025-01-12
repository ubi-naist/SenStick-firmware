//
//  SenStickDeviceManager.swift
//  SenStickSDK
//
//  Created by AkihiroUehara on 2016/03/15.
//  Copyright © 2016年 AkihiroUehara. All rights reserved.
//

import Foundation
import CoreBluetooth

// SenStickDeviceManagerクラスは、BLEデバイスの発見と、デバイスリストの管理を行うクラスです。シングルトンです。
//
// シングルトン
// このクラスはシングルトンです。sharedInstanceプロパティを通じてのみ、このクラスのインスタンスにアクセスできます。
// このクラスがシングルトンになっているのは、BLE接続管理は、アプリケーションで1つのCBCentralManagerクラスのインスタンスでまとめられるべきだからです。
// シングルトンにするために、init()メソッドはプライベートになっています。
// 
// デバイスの発見
// デバイスを発見するにはスキャンをします。scan()メソッドでスキャンが開始されます。1秒ごとに引数で渡したブロックが呼び出されます。
// スキャンを中止するにはcancelScan()メソッドを呼び出します。
// スキャンの状況は、isScanningプロパティで取得できます。
//
// デバイスのリスト管理
// デバイスが発見されるたびに、devicesプロパティが更新されます。devicesプロパティの更新はKVO(Key Value Observation)で取得できます。
//

open class SenStickDeviceManager : NSObject, CBCentralManagerDelegate
{
    let queue: DispatchQueue
    var manager: CBCentralManager?
    
    var scanTimer: DispatchSourceTimer?
    var scanCallback:((_ remaining: TimeInterval) -> Void)?
    
    // Properties, KVO
    dynamic open fileprivate(set) var devices:[SenStickDevice] = []
    
    // iOS10で、列挙型 CBCentralManagerStateがCBManagerStateに変更された。
    // 型名だけの変更で要素は変更がないので、Intの値としては、そのまま同じコードでiOS10以降も動く。
    // しかし強い型を使うSwiftでは、iOSのバージョンの差を吸収しなくてはならない。
    // 1つは、列挙型を引き継いで、バージョンごとにプロパティを提供する方法。
    // コメントアウトしたのがそのコード。これはアプリケーション側で、iOSのバージョンごとにプロパティを書き分けなくてはならない。手間だろう。
    // 綺麗ではないが、Int型をそのまま表に出すことにする。
    /*
    // iOS10でCBCentralManagerStateは廃止されてCBManagerStateが代替。型が異なるのでプロパティをそれぞれに分けている。
    var _state : Int = 0
    @available(iOS 10.0, *)
    dynamic open fileprivate(set) var state: CBManagerState {
        get {
            return CBManagerState(rawValue: _state) ?? .unknown
        }
        set(newState) {
            _state = newState.rawValue
        }
    }
    
    // iOS9まではCBCentralManagerStateを使う。
    @available(iOS, introduced: 5.0, deprecated: 10.0, message: "Use state instead")
    dynamic open fileprivate(set) var centralState: CBCentralManagerState {
        get {
            return CBCentralManagerState(rawValue: _state) ?? .unknown
        }
        set(newState) {
            _state = newState.rawValue
        }
    }
    */
    
    dynamic open fileprivate(set) var state: Int = 0
    
    dynamic open fileprivate(set) var isScanning: Bool = false
    
    // Initializer, Singleton design pattern.
    open static let sharedInstance: SenStickDeviceManager = SenStickDeviceManager()
    
    fileprivate override init() {
        queue = DispatchQueue(label: "senstick.ble-queue") // serial queue
        
        super.init()
        
        manager = CBCentralManager.init(delegate: self, queue: queue)
    }
    
    // MARK: Public methods
    
    // 1秒毎にコールバックします。0になれば終了です。
    open func scan(_ duration:TimeInterval = 5.0, callback:((_ remaining: TimeInterval) -> Void)?)
    {
        // デバイスリストをクリアする
        DispatchQueue.main.async(execute: {
            self.devices = []
        })
        
        // スキャン中、もしくはBTの電源がオフであれば、直ちに終了。
        if manager!.isScanning || manager!.state != .poweredOn {
            callback?(0)
            return
        }
        
        // スキャン時間は1秒以上、30秒以下に制約
        let scanDuration = max(1, min(30, duration))
        scanCallback = callback
        
        // 接続済のペリフェラルを取得する
        for peripheral in (manager!.retrieveConnectedPeripherals(withServices: [SenStickUUIDs.advertisingServiceUUID])) {
            addPeripheral(peripheral, name:nil)
        }
        
        // スキャンを開始する。
        manager!.scanForPeripherals(withServices: [SenStickUUIDs.advertisingServiceUUID], options: nil)
        isScanning = true
        
        var remaining = scanDuration
        scanTimer = DispatchSource.makeTimerSource(flags: DispatchSource.TimerFlags(rawValue: UInt(0)), queue: DispatchQueue.main)
        scanTimer?.scheduleRepeating(deadline: DispatchTime.now(), interval: 1.0) // 1秒ごとのタイマー
        scanTimer?.setEventHandler {
            // 時間を-1秒。
            remaining = max(0, remaining - 1)
            if remaining <= 0 {
                self.cancelScan()
            }
            // 継続ならばシグナリング
            self.scanCallback?(remaining)
        }
        scanTimer!.resume()
    }
    
    open func scan(_ duration:TimeInterval = 5.0)
    {
        scan(duration, callback: nil)
    }
    
    open func cancelScan()
    {
        guard manager!.isScanning else { return }
        
        scanTimer!.cancel()
        
        self.scanCallback?(0)
        self.scanCallback = nil
        
        self.manager!.stopScan()
        self.isScanning = false
    }
    
    // CentralManagerにデリゲートを設定して初期状態に戻します。
    // ファームウェア更新などで、CentralManagerを別に渡して利用した後、復帰するために使います。
    open func reset()
    {
        // デバイスリストをクリアする
        DispatchQueue.main.async(execute: {
            self.devices = []
        })

        manager?.delegate = self
    }
    
    // MARK: Private methods
    
    func addPeripheral(_ peripheral: CBPeripheral, name: String?)
    {
        //すでに配列にあるかどうか探す, なければ追加。KVOを活かすため、配列それ自体を代入する
        if !devices.contains(where: { element -> Bool in element.peripheral == peripheral }) {
            var devs = Array<SenStickDevice>(self.devices)
            devs.append(SenStickDevice(manager: self.manager!, peripheral: peripheral, name: name))
            DispatchQueue.main.async(execute: {
                self.devices = devs
            })
        }
    }
    
    // MARK: CBCentralManagerDelegate
    open func centralManagerDidUpdateState(_ central: CBCentralManager)
    {
        // BLEの処理は独立したキューで走っているので、KVOを活かすためにメインキューで代入する
        DispatchQueue.main.async(execute: {
            // iOS9以前とiOS10以降で、stateの列挙型の型名は異なるが、Intの値と要素はまったく同じ。
            // iOSのバージョンごとにプロパティを分けた場合は、コメントアウトのコードでバージョンに合わせて適合させられるが、使う側からすればややこしいだけか。
            /*
            if #available(iOS 10.0, *) {
                self.state = central.state
            } else { // iOS10以前
                self.centralState = CBCentralManagerState(rawValue:central.state.rawValue) ?? .unknown
            }
             */
            self.state = central.state.rawValue
        })
        
        switch central.state {
        case .poweredOn: break
        case .poweredOff:
            DispatchQueue.main.async(execute: {
                self.devices = []
            })
        case .unauthorized:
            DispatchQueue.main.async(execute: {
                self.devices = []
            })
        case .unknown:
            DispatchQueue.main.async(execute: {
                self.devices = []
            })
        case .unsupported:
            DispatchQueue.main.async(execute: {
                self.devices = []
            })
            break
        default: break
        }
    }
    
    open func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber)
    {
        DispatchQueue.main.async(execute: {
            self.addPeripheral(peripheral, name: advertisementData[CBAdvertisementDataLocalNameKey] as? String )
        })
    }
    
    open func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral)
    {
        for device in devices.filter({element -> Bool in element.peripheral == peripheral}) {
            DispatchQueue.main.async(execute: {
                device.onConnected()
            })
        }
    }
    
    open func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?)
    {
        for device in devices.filter({element -> Bool in element.peripheral == peripheral}) {
            DispatchQueue.main.async(execute: {
                device.onDisConnected()
            })
        }
    }
}
