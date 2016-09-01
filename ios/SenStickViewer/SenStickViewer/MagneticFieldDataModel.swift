//
//  MagneticFieldCell.swift
//  SenStickViewer
//
//  Created by AkihiroUehara on 2016/05/25.
//  Copyright © 2016年 AkihiroUehara. All rights reserved.
//

import UIKit
import SenStickSDK
import CoreMotion

class MagneticFieldDataModel : SensorDataModel
{
    weak var service: MagneticFieldSensorService? {
        didSet {
            self.service?.delegate = self
            didUpdateSetting(self)
        }
    }
    
    override init() {
        super.init()
        self.sensorName = "magnetic"
        self.csvHeader  = "Magnetic.X,\tMagnetic.Y,\tMagnetic.Z"
        self.csvEmptyData = ",\t,\t"
    }
    
    override func startToReadLog(logid: UInt8)
    {
        service?.readLogData()
        super.startToReadLog(logid)
        
        let logID = SensorLogID(logID: logid, skipCount: 0, position: 0)
        service?.writeLogID(logID)
    }
    
    // MARK: - SenStickSensorServiceDelegate
    override func didUpdateSetting(sender:AnyObject)
    {
        cell?.iconButton?.enabled = (self.service != nil)
        cell?.iconButton?.selected = (service?.settingData?.status != .Stopping)
        
        // レンジの更新
        self.maxValue = 100
        self.minValue = -100
        
        if let setting = service?.settingData {
            self.duration = setting.samplingDuration
        }
    }
    
    override func didUpdateRealTimeData(sender: AnyObject)
    {
        if let data = service?.realtimeData {
            drawRealTimeData([data.x, data.y, data.z])
        }
    }
    
    override func didUpdateMetaData(sender: AnyObject)
    {
        guard let metaData = service?.logMetaData  else {
            return
        }
        
        self.duration = metaData.samplingDuration
        // レンジの更新
        self.maxValue = 100
        self.minValue = -100
        
        let count = metaData.availableSampleCount
        cell?.graphView?.sampleCount = Int(count)
        cell?.iconButton?.enabled    = (count != 0)
        cell?.iconButton?.selected   = (count != 0)
        cell?.progressBar?.hidden    = (count == 0)
    }
    
    override func didUpdateLogData(sender: AnyObject)
    {
        if let array = service?.readLogData() {
            for data in array {
                addReadLog([data.x, data.y, data.z])
            }
        }
    }
    
    // MARK: - Event handler
    override func  iconButtonToutchUpInside(sender: UIButton) {
        let status :SenStickStatus = cell!.iconButton!.selected ? .Stopping : .SensingAndLogging
        
        if let current_setting = self.service?.settingData {
            let setting = SensorSettingData<MagneticFieldRange>(status: status, samplingDuration: current_setting.samplingDuration, range: current_setting.range)
            
            service?.writeSetting(setting)
        }
        service?.readSetting()
    }
}