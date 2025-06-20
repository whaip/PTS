# PCB故障检测系统 - 故障诊断算法详解

## 故障诊断算法概述

PCB故障检测系统采用多层次、多参数的诊断算法体系，结合传统信号处理方法和现代机器学习技术，对五种主要电子元件进行精确的故障检测和分类。

### 诊断算法架构

```
┌─────────────────────────────────────────────────────────────────┐
│                    故障诊断引擎 (FaultDiagnostic)                │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐  │
│  │   电阻器    │ │   电容器    │ │   电感器    │ │   二极管    │  │
│  │  诊断算法   │ │  诊断算法   │ │  诊断算法   │ │  诊断算法   │  │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘  │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐  │
│  │  集成电路   │ │  信号处理   │ │  统计分析   │ │  模式识别   │  │
│  │  诊断算法   │ │   工具库    │ │   工具库    │ │   工具库    │  │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│                      诊断结果处理                                │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐  │
│  │  置信度计算 │ │  故障分类   │ │  严重度评估 │ │  建议生成   │  │
│  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## 电阻器故障诊断算法

### 1. 电阻器故障类型

```cpp
enum ResistorFaultType {
    RESISTOR_NORMAL,          // 正常
    RESISTOR_OPEN,            // 开路
    RESISTOR_SHORT,           // 短路  
    RESISTOR_OUT_OF_TOLERANCE,// 阻值偏差
    RESISTOR_THERMAL_DRIFT,   // 热漂移
    RESISTOR_NOISE_EXCESSIVE, // 噪声过大
    RESISTOR_NONLINEAR        // 非线性特性
};
```

### 2. 电阻器测量方法

```cpp
class ResistorDiagnostic {
private:
    struct ResistorMeasurement {
        double dcResistance;        // 直流电阻
        double acImpedance;         // 交流阻抗  
        double noiseLevel;          // 噪声电平
        double temperatureCoeff;    // 温度系数
        QVector<double> frequencyResponse; // 频率响应
        double thermalNoise;        // 热噪声
    };

public:
    DiagnosticResult diagnoseResistor(const ComponentSpec& spec, 
                                     const MeasurementResult& measurement) {
        DiagnosticResult result(spec.getName());
        
        // 1. 提取测量参数
        ResistorMeasurement params = extractMeasurementParams(measurement);
        
        // 2. 基本故障检测
        if (detectOpenCircuit(params, spec)) {
            return createOpenCircuitResult(result, params);
        }
        
        if (detectShortCircuit(params, spec)) {
            return createShortCircuitResult(result, params);
        }
        
        // 3. 阻值偏差分析
        if (detectToleranceDeviation(params, spec)) {
            return createToleranceResult(result, params, spec);
        }
        
        // 4. 高级故障检测
        if (detectThermalDrift(params, spec)) {
            return createThermalDriftResult(result, params);
        }
        
        if (detectExcessiveNoise(params, spec)) {
            return createNoiseResult(result, params);
        }
        
        // 5. 频率特性分析
        if (detectNonlinearBehavior(params, spec)) {
            return createNonlinearResult(result, params);
        }
        
        // 6. 正常状态
        return createNormalResult(result, params);
    }

private:
    bool detectOpenCircuit(const ResistorMeasurement& params, 
                          const ComponentSpec& spec) {
        double openThreshold = spec.getParameter("open_threshold").toDouble();
        return params.dcResistance > openThreshold;
    }
    
    bool detectShortCircuit(const ResistorMeasurement& params,
                           const ComponentSpec& spec) {
        double shortThreshold = spec.getParameter("short_threshold").toDouble();
        return params.dcResistance < shortThreshold;
    }
    
    bool detectToleranceDeviation(const ResistorMeasurement& params,
                                 const ComponentSpec& spec) {
        double nominalValue = spec.getParameter("resistance").toDouble();
        double tolerance = spec.getParameter("tolerance").toDouble() / 100.0;
        
        double lowerBound = nominalValue * (1.0 - tolerance);
        double upperBound = nominalValue * (1.0 + tolerance);
        
        return (params.dcResistance < lowerBound) || 
               (params.dcResistance > upperBound);
    }
    
    bool detectThermalDrift(const ResistorMeasurement& params,
                           const ComponentSpec& spec) {
        double maxTempCoeff = spec.getParameter("max_temp_coeff").toDouble();
        return abs(params.temperatureCoeff) > maxTempCoeff;
    }
    
    bool detectExcessiveNoise(const ResistorMeasurement& params,
                             const ComponentSpec& spec) {
        double maxNoiseLevel = spec.getParameter("max_noise").toDouble();
        return params.noiseLevel > maxNoiseLevel;
    }
    
    bool detectNonlinearBehavior(const ResistorMeasurement& params,
                                const ComponentSpec& spec) {
        // 分析频率响应的线性度
        return analyzeFrequencyLinearity(params.frequencyResponse) < 0.95;
    }
    
    double analyzeFrequencyLinearity(const QVector<double>& response) {
        // 计算频率响应的线性相关系数
        return calculateCorrelationCoefficient(response);
    }
};
```

### 3. 电阻器诊断算法实现

```cpp
DiagnosticResult ResistorDiagnostic::createToleranceResult(
    DiagnosticResult& result, 
    const ResistorMeasurement& params,
    const ComponentSpec& spec) {
    
    double nominalValue = spec.getParameter("resistance").toDouble();
    double deviation = abs(params.dcResistance - nominalValue) / nominalValue * 100.0;
    
    result.setFaultType(DiagnosticResult::OUT_OF_TOLERANCE);
    result.setSeverity(DiagnosticResult::WARNING);
    result.setDescription(QString("电阻值偏差: %1% (测量值: %2Ω, 标称值: %3Ω)")
                         .arg(deviation, 0, 'f', 2)
                         .arg(params.dcResistance, 0, 'f', 1)
                         .arg(nominalValue, 0, 'f', 1));
    
    // 置信度计算基于偏差程度
    double tolerance = spec.getParameter("tolerance").toDouble();
    double confidence = calculateDeviationConfidence(deviation, tolerance);
    result.setConfidence(confidence);
    
    // 附加诊断信息
    result.setAdditionalData("measured_resistance", params.dcResistance);
    result.setAdditionalData("nominal_resistance", nominalValue);
    result.setAdditionalData("deviation_percent", deviation);
    result.setAdditionalData("tolerance_spec", tolerance);
    result.setAdditionalData("ac_impedance", params.acImpedance);
    result.setAdditionalData("noise_level", params.noiseLevel);
    
    return result;
}

double ResistorDiagnostic::calculateDeviationConfidence(double deviation, double tolerance) {
    double ratio = deviation / tolerance;
    
    if (ratio > 3.0) return 0.98;      // 严重偏差
    if (ratio > 2.0) return 0.92;      // 明显偏差
    if (ratio > 1.5) return 0.85;      // 中等偏差
    if (ratio > 1.2) return 0.75;      // 轻微偏差
    return 0.65;                       // 边界偏差
}
```

## 电容器故障诊断算法

### 1. 电容器故障类型

```cpp
enum CapacitorFaultType {
    CAPACITOR_NORMAL,           // 正常
    CAPACITOR_OPEN,             // 开路
    CAPACITOR_SHORT,            // 短路
    CAPACITOR_CAPACITANCE_LOW,  // 容量下降
    CAPACITOR_HIGH_ESR,         // 高ESR
    CAPACITOR_HIGH_LEAKAGE,     // 漏电流过大
    CAPACITOR_DIELECTRIC_LOSS,  // 介质损耗
    CAPACITOR_AGING,            // 老化
    CAPACITOR_POLARIZATION_ERROR // 极性错误
};
```

### 2. 电容器测量参数

```cpp
struct CapacitorMeasurement {
    double capacitance;         // 电容值
    double esr;                 // 等效串联电阻
    double leakageCurrent;      // 漏电流
    double dissipationFactor;   // 损耗因子
    double impedance;           // 阻抗
    QVector<double> frequencyResponse; // 频率特性
    double temperatureStability; // 温度稳定性
    double voltageCoefficient;  // 电压系数
};
```

### 3. 电容器诊断实现

```cpp
class CapacitorDiagnostic {
public:
    DiagnosticResult diagnoseCapacitor(const ComponentSpec& spec,
                                      const MeasurementResult& measurement) {
        DiagnosticResult result(spec.getName());
        CapacitorMeasurement params = extractCapacitorParams(measurement);
        
        // 1. 开路检测 - 容值极小
        if (params.capacitance < getNominalCapacitance(spec) * 0.01) {
            return createOpenCircuitResult(result, params);
        }
        
        // 2. 短路检测 - ESR极小且阻抗极小
        if (params.esr < 0.01 && params.impedance < 0.1) {
            return createShortCircuitResult(result, params);
        }
        
        // 3. 高ESR检测
        double esrThreshold = calculateESRThreshold(spec);
        if (params.esr > esrThreshold) {
            return createHighESRResult(result, params, esrThreshold);
        }
        
        // 4. 漏电流检测
        double leakageThreshold = spec.getParameter("leakage_threshold").toDouble();
        if (params.leakageCurrent > leakageThreshold) {
            return createLeakageResult(result, params, leakageThreshold);
        }
        
        // 5. 容值偏差检测
        if (detectCapacitanceDeviation(params, spec)) {
            return createCapacitanceDeviationResult(result, params, spec);
        }
        
        // 6. 介质损耗检测
        if (detectDielectricLoss(params, spec)) {
            return createDielectricLossResult(result, params);
        }
        
        // 7. 老化检测
        if (detectAging(params, spec)) {
            return createAgingResult(result, params);
        }
        
        return createNormalResult(result, params);
    }

private:
    double calculateESRThreshold(const ComponentSpec& spec) {
        double capacitance = spec.getParameter("capacitance").toDouble();
        QString type = spec.getParameter("type").toString();
        
        // 根据电容类型和容值确定ESR阈值
        if (type == "electrolytic") {
            return 1.0 / (capacitance * 1e6);  // 电解电容
        } else if (type == "ceramic") {
            return 0.1 / (capacitance * 1e6);  // 陶瓷电容
        } else if (type == "film") {
            return 0.01 / (capacitance * 1e6); // 薄膜电容
        }
        
        return 1.0; // 默认值
    }
    
    bool detectCapacitanceDeviation(const CapacitorMeasurement& params,
                                   const ComponentSpec& spec) {
        double nominal = getNominalCapacitance(spec);
        double tolerance = spec.getParameter("tolerance").toDouble() / 100.0;
        
        double lowerBound = nominal * (1.0 - tolerance);
        double upperBound = nominal * (1.0 + tolerance);
        
        return (params.capacitance < lowerBound) || 
               (params.capacitance > upperBound);
    }
    
    bool detectDielectricLoss(const CapacitorMeasurement& params,
                             const ComponentSpec& spec) {
        double maxDissipationFactor = spec.getParameter("max_dissipation").toDouble();
        return params.dissipationFactor > maxDissipationFactor;
    }
    
    bool detectAging(const CapacitorMeasurement& params,
                    const ComponentSpec& spec) {
        // 基于多个参数的综合判断
        double nominal = getNominalCapacitance(spec);
        double capacitanceDegradation = (nominal - params.capacitance) / nominal;
        
        // 容值下降超过20%且ESR增加
        return (capacitanceDegradation > 0.2) && 
               (params.esr > calculateESRThreshold(spec) * 2);
    }
};
```

### 4. 高级电容器分析

```cpp
class AdvancedCapacitorAnalysis {
public:
    // 频率特性分析
    CapacitorFrequencyAnalysis analyzeFrequencyResponse(
        const QVector<double>& frequencies,
        const QVector<double>& impedances) {
        
        CapacitorFrequencyAnalysis analysis;
        
        // 1. 找到谐振频率
        analysis.resonantFrequency = findResonantFrequency(frequencies, impedances);
        
        // 2. 计算品质因子
        analysis.qualityFactor = calculateQualityFactor(frequencies, impedances);
        
        // 3. 分析频率稳定性
        analysis.frequencyStability = analyzeFrequencyStability(frequencies, impedances);
        
        // 4. 检测寄生效应
        analysis.parasiticInductance = estimateParasiticInductance(frequencies, impedances);
        analysis.parasiticResistance = estimateParasiticResistance(frequencies, impedances);
        
        return analysis;
    }
    
    // 温度特性分析
    TemperatureCharacteristics analyzeTemperatureResponse(
        const QVector<double>& temperatures,
        const QVector<double>& capacitances) {
        
        TemperatureCharacteristics characteristics;
        
        // 1. 计算温度系数
        characteristics.tempCoefficient = calculateTemperatureCoefficient(
            temperatures, capacitances);
        
        // 2. 分析温度稳定性
        characteristics.stability = analyzeTemperatureStability(
            temperatures, capacitances);
        
        // 3. 检测温度异常点
        characteristics.anomalies = detectTemperatureAnomalies(
            temperatures, capacitances);
        
        return characteristics;
    }

private:
    double findResonantFrequency(const QVector<double>& freq, 
                                const QVector<double>& impedance) {
        int minIndex = 0;
        double minImpedance = impedance[0];
        
        for (int i = 1; i < impedance.size(); ++i) {
            if (impedance[i] < minImpedance) {
                minImpedance = impedance[i];
                minIndex = i;
            }
        }
        
        return freq[minIndex];
    }
    
    double calculateQualityFactor(const QVector<double>& freq,
                                 const QVector<double>& impedance) {
        double resonantFreq = findResonantFrequency(freq, impedance);
        double minImpedance = *std::min_element(impedance.begin(), impedance.end());
        
        // 找到3dB带宽
        double threshold = minImpedance * sqrt(2.0);
        double bandwidth = calculate3dBBandwidth(freq, impedance, threshold);
        
        return resonantFreq / bandwidth;
    }
};
```

## 电感器故障诊断算法

### 1. 电感器故障类型和测量

```cpp
enum InductorFaultType {
    INDUCTOR_NORMAL,            // 正常
    INDUCTOR_OPEN,              // 开路
    INDUCTOR_SHORT,             // 短路
    INDUCTOR_PARTIAL_SHORT,     // 部分短路
    INDUCTOR_SATURATION,        // 磁饱和
    INDUCTOR_CORE_LOSS,         // 磁芯损耗
    INDUCTOR_COUPLING_CHANGE,   // 耦合变化
    INDUCTOR_Q_DEGRADATION      // 品质因子下降
};

struct InductorMeasurement {
    double inductance;          // 电感值
    double dcResistance;        // 直流电阻
    double qualityFactor;       // 品质因子  
    double saturationCurrent;   // 饱和电流
    double coreLoss;            // 磁芯损耗
    QVector<double> frequencyResponse; // 频率特性
    double selfResonantFreq;    // 自谐振频率
    double couplingCoefficient; // 耦合系数
};
```

### 2. 电感器诊断实现

```cpp
class InductorDiagnostic {
public:
    DiagnosticResult diagnoseInductor(const ComponentSpec& spec,
                                     const MeasurementResult& measurement) {
        DiagnosticResult result(spec.getName());
        InductorMeasurement params = extractInductorParams(measurement);
        
        // 1. 开路检测
        if (params.dcResistance > 1e6) {  // 1MΩ
            return createOpenCircuitResult(result, params);
        }
        
        // 2. 短路检测
        if (params.inductance < getNominalInductance(spec) * 0.01) {
            return createShortCircuitResult(result, params);
        }
        
        // 3. 部分短路检测
        if (detectPartialShort(params, spec)) {
            return createPartialShortResult(result, params, spec);
        }
        
        // 4. 电感值偏差检测
        if (detectInductanceDeviation(params, spec)) {
            return createInductanceDeviationResult(result, params, spec);
        }
        
        // 5. 品质因子检测
        if (detectQFactorDegradation(params, spec)) {
            return createQFactorResult(result, params, spec);
        }
        
        // 6. 磁饱和检测
        if (detectSaturation(params, spec)) {
            return createSaturationResult(result, params);
        }
        
        // 7. 磁芯损耗检测
        if (detectCoreLoss(params, spec)) {
            return createCoreLossResult(result, params);
        }
        
        return createNormalResult(result, params);
    }

private:
    bool detectPartialShort(const InductorMeasurement& params,
                           const ComponentSpec& spec) {
        double nominalInductance = getNominalInductance(spec);
        double inductanceRatio = params.inductance / nominalInductance;
        
        // 电感值显著降低但未完全短路
        return (inductanceRatio < 0.7) && (inductanceRatio > 0.1);
    }
    
    bool detectQFactorDegradation(const InductorMeasurement& params,
                                 const ComponentSpec& spec) {
        double minQFactor = spec.getParameter("min_q_factor").toDouble();
        return params.qualityFactor < minQFactor;
    }
    
    bool detectSaturation(const InductorMeasurement& params,
                         const ComponentSpec& spec) {
        double nominalSatCurrent = spec.getParameter("saturation_current").toDouble();
        return params.saturationCurrent < nominalSatCurrent * 0.8;
    }
    
    bool detectCoreLoss(const InductorMeasurement& params,
                       const ComponentSpec& spec) {
        double maxCoreLoss = spec.getParameter("max_core_loss").toDouble();
        return params.coreLoss > maxCoreLoss;
    }
};
```

## 二极管故障诊断算法

### 1. 二极管故障类型和测量

```cpp
enum DiodeFaultType {
    DIODE_NORMAL,               // 正常
    DIODE_OPEN,                 // 开路
    DIODE_SHORT,                // 短路
    DIODE_HIGH_FORWARD_DROP,    // 正向压降过高
    DIODE_LOW_FORWARD_DROP,     // 正向压降过低
    DIODE_HIGH_REVERSE_LEAKAGE, // 反向漏电过大
    DIODE_BREAKDOWN_VOLTAGE,    // 击穿电压异常
    DIODE_JUNCTION_DAMAGE,      // 结损伤
    DIODE_THERMAL_DAMAGE        // 热损伤
};

struct DiodeMeasurement {
    double forwardVoltage;      // 正向压降
    double reverseLeakage;      // 反向漏电流
    double breakdownVoltage;    // 击穿电压
    double forwardCurrent;      // 正向电流
    double junctionCapacitance; // 结电容
    double thermalResistance;   // 热阻
    QVector<double> ivCurve;    // I-V特性曲线
    double idealityFactor;      // 理想因子
};
```

### 2. 二极管诊断实现

```cpp
class DiodeDiagnostic {
public:
    DiagnosticResult diagnoseDiode(const ComponentSpec& spec,
                                  const MeasurementResult& measurement) {
        DiagnosticResult result(spec.getName());
        DiodeMeasurement params = extractDiodeParams(measurement);
        
        // 1. 开路检测
        if (params.forwardCurrent < 1e-9 && params.reverseLeakage < 1e-12) {
            return createOpenCircuitResult(result, params);
        }
        
        // 2. 短路检测  
        if (params.forwardVoltage < 0.1 && params.reverseLeakage > 1e-3) {
            return createShortCircuitResult(result, params);
        }
        
        // 3. 正向压降检测
        if (detectForwardVoltageFault(params, spec)) {
            return createForwardVoltageResult(result, params, spec);
        }
        
        // 4. 反向漏电检测
        if (detectReverseLeakage(params, spec)) {
            return createReverseLeakageResult(result, params, spec);
        }
        
        // 5. 击穿电压检测
        if (detectBreakdownVoltage(params, spec)) {
            return createBreakdownResult(result, params, spec);
        }
        
        // 6. I-V特性分析
        if (analyzeIVCharacteristics(params, spec)) {
            return createIVAnalysisResult(result, params);
        }
        
        return createNormalResult(result, params);
    }

private:
    bool detectForwardVoltageFault(const DiodeMeasurement& params,
                                  const ComponentSpec& spec) {
        double nominalVf = spec.getParameter("forward_voltage").toDouble();
        double tolerance = spec.getParameter("vf_tolerance").toDouble();
        
        double lowerBound = nominalVf - tolerance;
        double upperBound = nominalVf + tolerance;
        
        return (params.forwardVoltage < lowerBound) || 
               (params.forwardVoltage > upperBound);
    }
    
    bool detectReverseLeakage(const DiodeMeasurement& params,
                             const ComponentSpec& spec) {
        double maxLeakage = spec.getParameter("max_reverse_leakage").toDouble();
        return params.reverseLeakage > maxLeakage;
    }
    
    bool analyzeIVCharacteristics(const DiodeMeasurement& params,
                                 const ComponentSpec& spec) {
        // 分析I-V曲线的理想性
        double idealityThreshold = spec.getParameter("max_ideality_factor").toDouble();
        return params.idealityFactor > idealityThreshold;
    }
    
    // 理想因子计算
    double calculateIdealityFactor(const QVector<QPair<double, double>>& ivData) {
        // 使用二极管方程拟合数据
        // I = Is * (exp(qV/nkT) - 1)
        // 其中n为理想因子
        
        QVector<double> voltages, currents;
        for (const auto& point : ivData) {
            if (point.second > 0) {  // 只考虑正向电流
                voltages.append(point.first);
                currents.append(log(point.second));
            }
        }
        
        // 线性拟合 ln(I) vs V
        return fitLinearSlope(voltages, currents) * kT_q;  // kT/q = 26mV at 300K
    }
};
```

## 集成电路故障诊断算法

### 1. IC故障类型和测量

```cpp
enum ICFaultType {
    IC_NORMAL,                  // 正常
    IC_POWER_PIN_FAULT,         // 电源引脚故障
    IC_GROUND_PIN_FAULT,        // 地引脚故障
    IC_INPUT_PIN_FAULT,         // 输入引脚故障
    IC_OUTPUT_PIN_FAULT,        // 输出引脚故障
    IC_INTERNAL_SHORT,          // 内部短路
    IC_LATCH_UP,                // 闩锁效应
    IC_THERMAL_DAMAGE,          // 热损伤
    IC_ESD_DAMAGE,              // 静电损伤
    IC_FUNCTIONAL_FAILURE       // 功能失效
};

struct ICMeasurement {
    QMap<int, double> pinVoltages;      // 引脚电压
    QMap<int, double> pinCurrents;      // 引脚电流
    QMap<int, double> pinResistances;   // 引脚电阻
    double powerConsumption;            // 功耗
    double operatingTemperature;        // 工作温度
    QMap<QString, bool> functionalTests; // 功能测试结果
    double propagationDelay;            // 传播延迟
    double outputDriveStrength;         // 输出驱动能力
};
```

### 2. IC诊断实现

```cpp
class ICDiagnostic {
public:
    DiagnosticResult diagnoseIC(const ComponentSpec& spec,
                               const MeasurementResult& measurement) {
        DiagnosticResult result(spec.getName());
        ICMeasurement params = extractICParams(measurement);
        
        // 1. 电源引脚检测
        if (detectPowerPinFault(params, spec)) {
            return createPowerPinResult(result, params, spec);
        }
        
        // 2. 接地引脚检测
        if (detectGroundPinFault(params, spec)) {
            return createGroundPinResult(result, params, spec);
        }
        
        // 3. 输入引脚检测
        if (detectInputPinFaults(params, spec)) {
            return createInputPinResult(result, params, spec);
        }
        
        // 4. 输出引脚检测
        if (detectOutputPinFaults(params, spec)) {
            return createOutputPinResult(result, params, spec);
        }
        
        // 5. 功能测试
        if (detectFunctionalFailures(params, spec)) {
            return createFunctionalResult(result, params, spec);
        }
        
        // 6. 闩锁效应检测
        if (detectLatchUp(params, spec)) {
            return createLatchUpResult(result, params);
        }
        
        // 7. 热损伤检测
        if (detectThermalDamage(params, spec)) {
            return createThermalDamageResult(result, params, spec);
        }
        
        return createNormalResult(result, params);
    }

private:
    bool detectPowerPinFault(const ICMeasurement& params,
                            const ComponentSpec& spec) {
        QVector<int> powerPins = spec.getParameter("power_pins").value<QVector<int>>();
        double nominalVoltage = spec.getParameter("supply_voltage").toDouble();
        double tolerance = spec.getParameter("voltage_tolerance").toDouble();
        
        for (int pin : powerPins) {
            double voltage = params.pinVoltages.value(pin, 0.0);
            double deviation = abs(voltage - nominalVoltage) / nominalVoltage;
            
            if (deviation > tolerance) {
                return true;
            }
        }
        
        return false;
    }
    
    bool detectFunctionalFailures(const ICMeasurement& params,
                                 const ComponentSpec& spec) {
        QStringList requiredTests = spec.getParameter("functional_tests").toStringList();
        
        for (const QString& test : requiredTests) {
            if (!params.functionalTests.value(test, false)) {
                return true;  // 功能测试失败
            }
        }
        
        return false;
    }
    
    bool detectLatchUp(const ICMeasurement& params,
                      const ComponentSpec& spec) {
        // 检测异常高电流消耗
        double maxCurrent = spec.getParameter("max_supply_current").toDouble();
        double actualCurrent = params.powerConsumption / 
                              spec.getParameter("supply_voltage").toDouble();
        
        return actualCurrent > maxCurrent * 10;  // 电流异常增大
    }
};
```

## 高级诊断算法

### 1. 机器学习增强诊断

```cpp
class MLEnhancedDiagnostic {
public:
    struct FeatureVector {
        QVector<double> timedomainFeatures;   // 时域特征
        QVector<double> freqdomainFeatures;   // 频域特征
        QVector<double> statisticalFeatures;  // 统计特征
        QVector<double> physicalFeatures;     // 物理特征
    };
    
    DiagnosticResult diagnoseWithML(const ComponentSpec& spec,
                                   const MeasurementResult& measurement) {
        // 1. 特征提取
        FeatureVector features = extractFeatures(measurement);
        
        // 2. 特征标准化
        FeatureVector normalizedFeatures = normalizeFeatures(features);
        
        // 3. 模型推理
        MLPrediction prediction = runInference(normalizedFeatures, spec.getType());
        
        // 4. 结果解释
        DiagnosticResult result = interpretPrediction(prediction, spec);
        
        return result;
    }

private:
    FeatureVector extractFeatures(const MeasurementResult& measurement) {
        FeatureVector features;
        
        // 时域特征提取
        features.timedomainFeatures = extractTimeDomainFeatures(measurement);
        
        // 频域特征提取
        features.freqdomainFeatures = extractFrequencyDomainFeatures(measurement);
        
        // 统计特征提取
        features.statisticalFeatures = extractStatisticalFeatures(measurement);
        
        // 物理特征提取
        features.physicalFeatures = extractPhysicalFeatures(measurement);
        
        return features;
    }
    
    QVector<double> extractTimeDomainFeatures(const MeasurementResult& measurement) {
        QVector<double> features;
        QVector<double> voltage = measurement.getVoltageData();
        QVector<double> current = measurement.getCurrentData();
        
        // 基本统计量
        features.append(calculateMean(voltage));
        features.append(calculateStdDev(voltage));
        features.append(calculateSkewness(voltage));
        features.append(calculateKurtosis(voltage));
        
        // 峰值特征
        features.append(findPeakValue(voltage));
        features.append(calculateCrestFactor(voltage));
        
        // 能量特征  
        features.append(calculateRMSValue(voltage));
        features.append(calculateTotalEnergy(voltage));
        
        return features;
    }
    
    QVector<double> extractFrequencyDomainFeatures(const MeasurementResult& measurement) {
        QVector<double> features;
        QVector<double> voltage = measurement.getVoltageData();
        
        // FFT计算
        QVector<std::complex<double>> fftResult = performFFT(voltage);
        QVector<double> powerSpectrum = calculatePowerSpectrum(fftResult);
        
        // 频域特征
        features.append(findDominantFrequency(powerSpectrum));
        features.append(calculateSpectralCentroid(powerSpectrum));
        features.append(calculateSpectralSpread(powerSpectrum));
        features.append(calculateSpectralEntropy(powerSpectrum));
        
        // 谐波分析
        features.append(calculateTHD(powerSpectrum));
        features.append(calculateSNR(powerSpectrum));
        
        return features;
    }
};
```

### 2. 自适应阈值算法

```cpp
class AdaptiveThresholdDiagnostic {
private:
    struct ThresholdHistory {
        QVector<double> values;
        QVector<QDateTime> timestamps;
        double currentThreshold;
        double confidence;
    };
    
    QMap<QString, ThresholdHistory> m_thresholdHistory;

public:
    double calculateAdaptiveThreshold(const QString& parameter,
                                     double currentValue,
                                     const ComponentSpec& spec) {
        ThresholdHistory& history = m_thresholdHistory[parameter];
        
        // 添加新数据点
        history.values.append(currentValue);
        history.timestamps.append(QDateTime::currentDateTime());
        
        // 保持历史数据量
        if (history.values.size() > 1000) {
            history.values.removeFirst();
            history.timestamps.removeFirst();
        }
        
        // 计算自适应阈值
        if (history.values.size() < 10) {
            // 数据不足，使用默认阈值
            return spec.getParameter(parameter + "_threshold").toDouble();
        }
        
        // 使用统计方法计算阈值
        double mean = calculateMean(history.values);
        double stdDev = calculateStdDev(history.values);
        
        // 3-sigma准则
        double upperThreshold = mean + 3.0 * stdDev;
        double lowerThreshold = mean - 3.0 * stdDev;
        
        // 考虑趋势变化
        double trend = calculateTrend(history.values);
        if (abs(trend) > 0.1) {
            // 存在明显趋势，调整阈值
            upperThreshold += trend * history.values.size();
            lowerThreshold += trend * history.values.size();
        }
        
        history.currentThreshold = upperThreshold;
        history.confidence = calculateThresholdConfidence(history.values);
        
        return upperThreshold;
    }

private:
    double calculateTrend(const QVector<double>& values) {
        if (values.size() < 5) return 0.0;
        
        // 使用最小二乘法计算趋势
        QVector<double> x, y;
        for (int i = 0; i < values.size(); ++i) {
            x.append(i);
            y.append(values[i]);
        }
        
        return calculateLinearSlope(x, y);
    }
    
    double calculateThresholdConfidence(const QVector<double>& values) {
        if (values.size() < 10) return 0.5;
        
        // 基于数据分布的置信度计算
        double mean = calculateMean(values);
        double stdDev = calculateStdDev(values);
        double cv = stdDev / mean;  // 变异系数
        
        // 变异系数越小，置信度越高
        return 1.0 / (1.0 + cv);
    }
};
```

### 3. 多参数融合诊断

```cpp
class MultiParameterFusionDiagnostic {
public:
    struct ParameterWeight {
        QString name;
        double weight;
        double reliability;
        double sensitivity;
    };
    
    DiagnosticResult fuseDiagnosticResults(
        const QVector<DiagnosticResult>& individualResults,
        const QVector<ParameterWeight>& weights) {
        
        DiagnosticResult fusedResult("FusedDiagnosis");
        
        // 1. 计算加权故障概率
        double totalFaultProbability = 0.0;
        double totalWeight = 0.0;
        
        for (int i = 0; i < individualResults.size(); ++i) {
            double confidence = individualResults[i].getConfidence();
            double weight = weights[i].weight * weights[i].reliability;
            
            if (individualResults[i].hasFault()) {
                totalFaultProbability += confidence * weight;
            }
            totalWeight += weight;
        }
        
        double fusedConfidence = totalFaultProbability / totalWeight;
        
        // 2. 确定主要故障类型
        DiagnosticResult::FaultType dominantFault = determineDominantFault(
            individualResults, weights);
        
        // 3. 计算严重程度
        DiagnosticResult::Severity fusedSeverity = calculateFusedSeverity(
            individualResults, weights);
        
        // 4. 生成融合描述
        QString fusedDescription = generateFusedDescription(
            individualResults, weights);
        
        fusedResult.setFaultType(dominantFault);
        fusedResult.setSeverity(fusedSeverity);
        fusedResult.setConfidence(fusedConfidence);
        fusedResult.setDescription(fusedDescription);
        
        return fusedResult;
    }

private:
    DiagnosticResult::FaultType determineDominantFault(
        const QVector<DiagnosticResult>& results,
        const QVector<ParameterWeight>& weights) {
        
        QMap<DiagnosticResult::FaultType, double> faultScores;
        
        for (int i = 0; i < results.size(); ++i) {
            if (results[i].hasFault()) {
                DiagnosticResult::FaultType type = results[i].getFaultType();
                double score = results[i].getConfidence() * weights[i].weight;
                faultScores[type] += score;
            }
        }
        
        // 找到得分最高的故障类型
        DiagnosticResult::FaultType dominantType = DiagnosticResult::NO_FAULT;
        double maxScore = 0.0;
        
        for (auto it = faultScores.begin(); it != faultScores.end(); ++it) {
            if (it.value() > maxScore) {
                maxScore = it.value();
                dominantType = it.key();
            }
        }
        
        return dominantType;
    }
};
```

---

*本文档详细介绍了PCB故障检测系统中各种电子元件的故障诊断算法，包括传统信号处理方法和现代机器学习技术的应用。这些算法共同构成了系统的核心诊断能力。*
