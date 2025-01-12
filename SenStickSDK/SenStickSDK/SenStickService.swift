//
//  SenStickService.swift
//  SenStickSDK
//
//  Created by AkihiroUehara on 2016/03/29.
//  Copyright © 2016年 AkihiroUehara. All rights reserved.
//

import Foundation
import CoreBluetooth

// BLEのサービスを一律して扱うためのプロトコルです。ライブラリを使う側が、使うことはありません。

protocol SenStickService
{
    init?(device: SenStickDevice)
    
    // 値が更新される都度CBPeripheralDelegateから呼びだされます
    func didUpdateValue(_ characteristic: CBCharacteristic, data:[UInt8])
}
