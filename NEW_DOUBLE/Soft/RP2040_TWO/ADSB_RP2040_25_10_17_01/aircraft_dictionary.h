#ifndef AIRCRAFT_DICTIONARY_HH_
#define AIRCRAFT_DICTIONARY_HH_

#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "Arduino.h"
#include "hal.h"
#include "json_utils.h"
#include "macros.h"
#include "transponder_packet.h"

#define FILTER_CPR_POSITIONS

class Aircraft1090 {
   public:
    static constexpr uint16_t kCallSignMaxNumChars      = 8;
    static constexpr uint16_t kCallSignMinNumChars      = 3;         // Позывные должны быть именно такой длительности, чтобы быть действительными.

    // Эти переменные определяют границы фильтра для времени между пакетами CPR. Если время между пакетами превышает
    // ограничение по временной разнице, старый пакет CPR отбрасывается, и пара пакетов CPR не используется для декодирования позиции.
    static constexpr uint32_t kDefaultCPRIntervalMs     = 10e3;      // Интервал CPR при запуске с нуля или устаревшего трека.
    static constexpr uint32_t kRefCPRIntervalMs         = 19e3;      // Референтный интервал для отклонения пар пакетов CPR.
    static constexpr uint32_t kMaxCPRIntervalMs         = 30e3;      // Никогда не принимайте пары пакетов CPR с интервалом более 30 секунд.
    static constexpr uint32_t kMaxTrackUpdateIntervalMs = 20e3;      // Треки старше этого срока считаются устаревшими.

    enum Category : uint8_t {
        kCategoryInvalid = 0,
        kCategoryReserved,
        kCategoryNoCategoryInfo,
        kCategorySurfaceEmergencyVehicle,
        kCategorySurfaceServiceVehicle,
        kCategoryGroundObstruction,
        kCategoryGliderSailplane,
        kCategoryLighterThanAir,
        kCategoryParachutistSkydiver,
        kCategoryUltralightHangGliderParaglider,
        kCategoryUnmannedAerialVehicle,
        kCategorySpaceTransatmosphericVehicle,
        kCategoryLight,                         // < 7000kg
        kCategoryMedium1,                       // 7000kg - 34000kg
        kCategoryMedium2,                       // 34000kg - 136000kg
        kCategoryHighVortexAircraft,
        kCategoryHeavy,                         // > 136000kg
        kCategoryHighPerformance,               // >5g acceleration and >400kt speed
        kCategoryRotorcraft
    };

    enum AltitudeSource : int16_t {
        kAltitudeNotAvailable = -2,
        kAltitudeSourceNotSet = -1,
        kAltitudeSourceBaro = 0,
        kAltitudeSourceGNSS = 1
    };

    enum VerticalRateSource : int16_t {
        kVerticalRateNotAvailable = -2,
        kVerticalRateSourceNotSet = -1,
        kVerticalRateSourceGNSS = 0,
        kVerticalRateSourceBaro = 1
    };

    enum VelocitySource : int16_t {
        kVelocitySourceNotAvailable = -2,
        kVelocitySourceNotSet = -1,
        kVelocitySourceGroundSpeed = 0,
        kVelocitySourceAirspeedTrue = 1,
        kVelocitySourceAirspeedIndicated = 2
    };

    enum BitFlag : uint32_t {
        kBitFlagIsAirborne = 0,            // Получены сообщения или флаги, указывающие на то, что самолет находится в воздухе.
        kBitFlagBaroAltitudeValid,
        kBitFlagGNSSAltitudeValid,
        kBitFlagPositionValid,
        kBitFlagDirectionValid,
        kBitFlagHorizontalVelocityValid,
        kBitFlagVerticalVelocityValid,
        kBitFlagIsMilitary,                // Получено по крайней мере одно военное сообщение ES с самолета.
        kBitFlagIsClassB2GroundVehicle,    // Является наземным транспортным средством класса B2, передающим мощность <70 Вт.
        kBitFlagHas1090ESIn,               // Самолет оснащен возможностью приема расширенного сквиттера на частоте 1090 МГц.
        kBitFlagHasUATIn,                  // Самолеты могут принимать UAT.
        kBitFlagTCASOperational,           // Система TCAS на самолете работоспособна.
        kBitFlagSingleAntenna,             // Указывает, что самолёт использует одну антенну. Передача данных может быть прерывистой.
        kBitFlagDirectionIsHeading,        // Направление — это курс самолета, а не угол траектории.
        kBitFlagHeadingUsesMagneticNorth,  // Направление в сообщениях о местоположении на поверхности и в воздухе указывает на магнитный север, а не на истинный север.
        kBitFlagIdent,                     // Переключатель IDENT в данный момент активен.
        kBitFlagAlert,                     // Самолет подает сигнал тревоги.
        kBitFlagTCASRA,                    // Указывает на то, что активна рекомендация по разрешению ситуации TCAS.
        kBitFlagReserved0,
        kBitFlagReserved1,
        kBitFlagReserved2,
        kBitFlagReserved3,

        // Флаги после kBitFlagUpdatedBaroAltitude очищаются в конце каждого отчетного интервала.
        kBitFlagUpdatedBaroAltitude,
        kBitFlagUpdatedGNSSAltitude,
        kBitFlagUpdatedPosition,
        kBitFlagUpdatedDirection,
        kBitFlagUpdatedHorizontalVelocity,
        kBitFlagUpdatedVerticalVelocity,
        kBitFlagNumFlagBits
    };

    enum NICBit : uint16_t { kNICBitA = 0, kNICBitB = 1, kNICBitC = 2 };

    enum SystemDesignAssurance : uint8_t {
        kSDASupportedFailureUnknownOrNoSafetyEffect = 0b00,
        kSDASupportedFailureMinor = 0b01,
        kSDASupportedFailureMajor = 0b10,
        kSDASupportedFailureHazardous = 0b11
    };

    enum NICRadiusOfContainment : uint8_t {
        kROCUnknown = 0,
        kROCLessThan20NauticalMiles = 1,
        kROCLessThan8NauticalMiles = 2,
        kROCLessThan4NauticalMiles = 3,
        kROCLessThan2NauticalMiles = 4,
        kROCLessThan1NauticalMile = 5,
        kROCLessThan0p6NauticalMiles = 6,  // Lump together with <0.5NM and <0.3NM since they share a NIC value.
        kROCLessThan0p2NauticalMiles = 7,
        kROCLessThan0p1NauticalMiles = 8,
        kROCLessThan75Meters = 9,
        kROCLessThan25Meters = 10,
        kROCLessThan7p5Meters = 11
    };

    enum NICBarometricAltitudeIntegrity : uint8_t {
        kBAIGillhamInputNotCrossChecked = 0,
        kBAIGillHamInputCrossCheckedOrNonGillhamSource = 1
    };

    enum NACHorizontalVelocityError : uint8_t {
        kHVEUnknownOrGreaterThanOrEqualTo10MetersPerSecond = 0b000,
        kHVELessThan10MetersPerSecond = 0b110,
        kHVELessThan3MetersPerSecond = 0b010,
        kHVELessThan1MeterPerSecond = 0b011,
        kHVELessThan0p3MetersPerSecond = 0b100
    };

    enum NACEstimatedPositionUncertainty : uint8_t {
        kEPUUnknownOrGreaterThanOrEqualTo10NauticalMiles = 0,
        kEPULessThan10NauticalMiles = 1,
        kEPULessThan4NauticalMiles = 2,
        kEPULessThan2NauticalMiles = 3,
        kEPULessThan1NauticalMile = 4,
        kEPULessThan0p5NauticalMiles = 5,
        kEPULessThan0p3NauticalMiles = 6,
        kEPULessThan0p1NauticalMiles = 7,
        kEPULessThan0p05NauticalMiles = 8,
        kEPULessThan30Meters = 9,
        kEPULessThan10Meters = 10,
        kEPULessThan3Meters = 11
    };

    // Составное значение SIL равно SIL | (SIL_supplement << 2).
    enum SILProbabilityOfExceedingNICRadiusOfContainmnent : uint8_t {
        kPOERCUnknownOrGreaterThan1em3PerFlightHour = 0b000,
        kPOERCLessThanOrEqualTo1em3PerFligthHour = 0b001,
        kPOERCLessThanOrEqualTo1em5PerFlightHour = 0b010,
        kPOERCLessThanOrEqualTo1em7PerFlightHour = 0b011,
        kPOERCUnknownOrGreaterThan1em3PerSample = 0b100,
        kPOERCLessThanOrEqualTo1em3PerSample = 0b101,
        kPOERCLessThanOrEqualTo1em5PerSample = 0b110,
        kPOERCLessThanOrEqualTo1em7PerSample = 0b111,
    };

    enum GVA : uint8_t {
        kGVAUnknownOrGreaterThan150Meters = 0,
        GVALessThanOrEqualTo150Meters = 1,
        GVALessThanOrEqualTo45Meters = 2,
        GVALessThan45Meters = 3
    };

    struct Metrics {
        // Мы можем только подтвердить, что достоверные кадры были получены с этого самолёта. Неизвестно, с кого были получены недействительные кадры.
        uint16_t valid_squitter_frames = 0;
        uint16_t valid_extended_squitter_frames = 0;
    };

    Aircraft1090(uint32_t icao_address_in); 
    Aircraft1090();

    /**
      * Проверяет, можно ли декодировать местоположение воздушного судна. Требует, чтобы были получены как чётный, так и нечётный пакет,
      * и чтобы между ними не было слишком большого интервала времени.
      * @retval True, если местоположение воздушного судна можно декодировать, в противном случае — false.
      */
    bool CanDecodePosition();

    /**
    * Очищает кэш пакетов CPR. Используется, когда с момента получения последнего пакета CPR прошло слишком много времени, чтобы избежать
    * декодирования местоположения CPR с недопустимыми парами пакетов.
    */
    inline void ClearCPRPackets() 
    {
        // Очистка полученных временных меток приводит к отклонению пары пакетов на этапе декодирования, поэтому это так же эффективно,
        // как и полное удаление содержимого полученных пакетов.
        last_odd_packet_.received_timestamp_ms = 0;
        last_even_packet_.received_timestamp_ms = 0;
    }

    /**
    * Декодирует местоположение воздушного судна с использованием last_odd_packet_ и last_even_packet_.
    * @param[in] filter_cpr_position True, если необходимо запустить фильтр местоположения CPR (по умолчанию true).
    * @retval True, если местоположение декодировано успешно, в противном случае false.
    */
    bool DecodePosition(bool filter_cpr_position = true);
    /**
    * Возвращает максимальную разницу во времени между пакетами CPR, которая будет принята для декодирования.
    * @retval Максимально допустимая разница во времени между пакетами CPR.
    */
    uint32_t GetMaxAllowedCPRIntervalMs() const {
        if (velocity_source == kVelocitySourceNotSet || velocity_source == kVelocitySourceNotAvailable || /* millis()*/ get_time_since_boot_ms() - last_track_update_timestamp_ms > kMaxTrackUpdateIntervalMs) 
        {
            return kDefaultCPRIntervalMs;
        }
        // Масштабируйте пороговое значение дельты времени на основе скорости самолета относительно 500 узлов, но ограничьте результат
      // максимальными пороговыми значениями дельты времени.
        return MIN(kRefCPRIntervalMs * 500 / velocity_kts, kMaxCPRIntervalMs);
    }
    /**
    * Проверяет, установлен ли бит-флаг.
    * @param[in] bit Положение проверяемого бита.
    * @retval True, если бит установлен, false, если бит сброшен.
    */
    inline bool HasBitFlag(BitFlag bit) const { return flags & (0b1 << bit) ? true : false; }
    /**
    * Указывает на получение кадра путем увеличения соответствующего счетчика кадров.
    * @param[in] is_extended_squitter Устанавливается в значение true, если полученный кадр был в режиме S.
    */
    inline void IncrementNumFramesReceived(bool is_extended_squitter = false) 
    {
        is_extended_squitter ? metrics_counter_.valid_extended_squitter_frames++
                             : metrics_counter_.valid_squitter_frames++;
    }

    /**
    * Возвращает, был ли записан дополнительный бит сетевой карты. Используется для определения, когда использовать дополнительные биты сетевой карты для
    * определения значения сетевой карты на основе полученных кодов типов.
    * @param[in] бит Дополнительный бит сетевой карты для проверки.
    * @retval True, если бит был записан, false в противном случае.
    */
    inline bool NICBitIsValid(NICBit bit) { return nic_bits & (0b1 << bit); }

    /**
    * Сбрасывает только биты флагов, которые показывают, что что-то было обновлено в течение последнего отчетного интервала.
     */
    inline void ResetUpdatedBitFlags() { flags &= ~(~0b0 << kBitFlagUpdatedBaroAltitude); }

    /**
      * Задаёт местоположение воздушного судна в формате компактного отчёта о местоположении (CPR). Принимает чётный или нечётный набор широты/долготы
      * и использует их для определения местоположения воздушного судна.
      * @param[in] n_lat_cpr 17-битное значение широты.
      * @param[in] n_lon_cpr 17-битное значение долготы.
      * @param[in] нечётное логическое значение, указывающее, что обновление местоположения выполняется относительно нечётной координатной сетки (если true) или
      * чётной координатной сетки.
      * @param[in] received_timestamp_ms Временная метка в миллисекундах на момент получения пакета местоположения. Должна быть получена
      * из счётчика MLAT, но точность может быть довольно низкой, поскольку она используется только при принятии решения об отклонении пар пакетов CPR
      * из-за слишком большого времени между чётными и нечётными пакетами.
      * @retval True, если координаты были проанализированы успешно, false в противном случае. ПРИМЕЧАНИЕ: Неверные позиции всё ещё могут
      * считаться успешным разбором.
      */
    bool SetCPRLatLon(uint32_t n_lat_cpr, uint32_t n_lon_cpr, bool odd, uint32_t received_timestamp_ms);

    /**
    * Перенести счётчик метрик в поле общедоступных метрик.
    */
    inline void UpdateMetrics() 
    {
        metrics = metrics_counter_;
        metrics_counter_ = Metrics();
    }

    /**
     * Set or clear a bit on the Aircraft.
     */
    inline void WriteBitFlag(BitFlag bit, bool value) { value ? flags |= (0b1 << bit) : flags &= ~(0b1 << bit); }

    /**
     * Write a value for a NIC supplement bit. Used to piece together a NIC from separate messages, so that the NIC can
     * be determined based on a received TypeCode.
     * @param[in] bit NIC supplement bit to write.
     * @param[in] value Value to write to the bit.
     */
    inline void WriteNICBit(NICBit bit, bool value) 
    {
        value ? nic_bits |= (0b1 << bit) : nic_bits &= ~(0b1 << bit);
        // FIXME: Permanently setting NIC bits valid like this can cause invalid navigation integrity values to be read
        // if a stale NIC supplement bit is being used. Hopefully this isn't a big problem if NIC values don't change
        // frequently.
        nic_bits_valid |= (0b1 << bit);
    }

    uint32_t flags = 0b0;

    uint32_t last_message_timestamp_ms       = 0;
    int16_t last_message_signal_strength_dbm = 0;    // Напряжение сигнала RSSI во время приема сообщения.
    int16_t last_message_signal_quality_db   = 0;    // Отношение RSSI к уровню шума во время приема сообщения.
    uint32_t last_track_update_timestamp_ms  = 0;    // Отметка времени последнего обновления позиции.
    Metrics metrics;

    uint16_t transponder_capability = 0;
    uint32_t icao_address = 0;
    char callsign[kCallSignMaxNumChars + 1] = "?";  // put extra EOS character at end
    uint16_t squawk = 0;
    Category category = kCategoryNoCategoryInfo;
    uint8_t category_raw = 0;  // Non-enum category in case we want the value without a many to one mapping.

    int32_t baro_altitude_ft = 0;
    int32_t gnss_altitude_ft = 0;
    int32_t gnss_altitude_m = 0;
    AltitudeSource altitude_source = kAltitudeSourceNotSet;

    // Airborne Position Message
    float latitude_deg = 0.0f;
    float longitude_deg = 0.0f;

    // Airborne Velocities Message
    float direction_deg = 0.0f;
    float velocity_kts = 0;
    VelocitySource velocity_source = kVelocitySourceNotSet;
    int vertical_rate_fpm = 0.0f;
    VerticalRateSource vertical_rate_source = kVerticalRateSourceNotSet;

    // Aircraft Operation Status Message
    // Navigation Integrity Category (NIC)
    uint8_t nic_bits_valid = 0b000;  // MSb to LSb: nic_c_valid nic_b_valid nic_a_valid.
    uint8_t nic_bits = 0b000;        // MSb to LSb: nic_c nic_b nic_a.
    NICRadiusOfContainment navigation_integrity_category = kROCUnknown;  // 4 bits.
    NICBarometricAltitudeIntegrity navigation_integrity_category_baro = kBAIGillhamInputNotCrossChecked;  // 1 bit. Default to worst case.
    // Navigation Accuracy Category (NAC)
    NACHorizontalVelocityError navigation_accuracy_category_velocity = kHVEUnknownOrGreaterThanOrEqualTo10MetersPerSecond;  // 3 bits.
    NACEstimatedPositionUncertainty navigation_accuracy_category_position = kEPUUnknownOrGreaterThanOrEqualTo10NauticalMiles;  // 4 bits.
    // Geometric Vertical Accuracy (GVA)
    GVA geometric_vertical_accuracy = kGVAUnknownOrGreaterThan150Meters;  // 2 bits.
    SILProbabilityOfExceedingNICRadiusOfContainmnent source_integrity_level = kPOERCUnknownOrGreaterThan1em3PerFlightHour;  // 3 bits.
    // System Design Assurance
    SystemDesignAssurance system_design_assurance = kSDASupportedFailureUnknownOrNoSafetyEffect;  // 2 bits.
    // GPS Antenna Offset
    int8_t gnss_antenna_offset_right_of_roll_axis_m = INT8_MAX;  // Defaults to INT8_MAX to indicate it hasn't been read yet.
    // Aircraft dimensions (on the ground).
    uint16_t length_m = 0;
    uint16_t width_m = 0;

    int8_t adsb_version = -1;

   private:
    struct CPRPacket {
        // SetCPRLatLon values.
        uint32_t received_timestamp_ms = 0;  // [ms] time since boot when packet was recorded
        uint32_t n_lat = 0;                  // 17-bit latitude count
        uint32_t n_lon = 0;                  // 17-bit longitude count
    };

    CPRPacket last_odd_packet_;
    CPRPacket last_even_packet_;

#ifdef FILTER_CPR_POSITIONS
    // Положение в альтернативном взвешенном двоичном коде. Устанавливается в соответствии с предполагаемым положением, которое может не совпадать с фактическими
    // отображаемыми значениями широты и логарифма. Формат — AWB, что позволяет быстро выполнять операции с фиксированной точкой для отбора
    // предполагаемых положений.
    uint32_t last_filter_received_timestamp_ms_ =  0;  // received_timestamp_ms для последнего пакета, переданного фильтру.
    uint32_t lat_awb_ = 0;
    uint32_t lon_awb_ = 0;
#endif

    Metrics metrics_counter_;
};

class AircraftDictionary {
   public:
    static const uint16_t kMaxNumAircraft = 400;
    static const uint16_t kMaxNumSources = 3;

    struct AircraftDictionaryConfig_t {
        uint32_t aircraft_prune_interval_ms = 60e3;
#ifdef FILTER_CPR_POSITIONS
        // Фильтр положения CPR сверяет каждое новое местоположение воздушного судна с предыдущим и требует два
        // последовательных пакета в пределах географического радиуса для подтверждения больших скачков местоположения воздушного судна. Это снижает
        // вероятность того, что воздушное судно может «спрыгнуть», но увеличивает нагрузку на процессор.
        bool enable_cpr_position_filter = true;
#endif
    };

    struct Metrics {
        static const uint16_t kMetricsJSONMaxLen = 1000;  // Includes null terminator.

        uint32_t raw_squitter_frames = 0;
        uint32_t valid_squitter_frames = 0;
        uint32_t raw_extended_squitter_frames = 0;
        uint32_t valid_extended_squitter_frames = 0;
        uint32_t demods_1090 = 0;

        uint16_t raw_squitter_frames_by_source[kMaxNumSources] = {0};
        uint16_t valid_squitter_frames_by_source[kMaxNumSources] = {0};
        uint16_t raw_extended_squitter_frames_by_source[kMaxNumSources] = {0};
        uint16_t valid_extended_squitter_frames_by_source[kMaxNumSources] = {0};
        uint16_t demods_1090_by_source[kMaxNumSources] = {0};

        /**
         * Formats the metrics dictionary into a JSON packet with the following structure.
         * {
         *      "raw_squitter_frames": 10,
         *      "valid_squitter_frames": 7,
         *      "raw_extended_squitter_frames": 30,
         *      "valid_extended_squitter_frames": 16,
         *      "demods_1090": 50,
         *      "raw_squitter_frames_by_source": [3, 3, 4],
         *      "valid_squitter_frames_by_source": [2, 2, 3],
         *      "raw_extended_squitter_frames_by_source": [10, 11, 9],
         *      "valid_squitter_frames_by_source": [4, 4, 8],
         *      "demods_1090_by_source": [20, 10, 20]
         * }
         * @param[in] buf Buffer to write the JSON string to.
         * @param[in] buf_len Length of the buffer, including the null terminator.
         */
        inline uint16_t ToJSON(char *buf, size_t buf_len) 
        {
            uint16_t message_max_len = buf_len - 1;  // Leave space for null terminator.
            snprintf(buf, message_max_len - strlen(buf),
                     "{ \"raw_squitter_frames\": %lu, \"valid_squitter_frames\": %lu, "
                     "\"raw_extended_squitter_frames\": %lu, "
                     "\"valid_extended_squitter_frames\": %lu, \"demods_1090\": %lu, ",
                     raw_squitter_frames, valid_squitter_frames, raw_extended_squitter_frames,
                     valid_extended_squitter_frames, demods_1090);
            uint16_t chars_written = strlen(buf);
            chars_written += ArrayToJSON(buf + chars_written, buf_len - chars_written, "raw_squitter_frames_by_source",
                                         raw_squitter_frames_by_source, "%u", true);
            chars_written +=
                ArrayToJSON(buf + chars_written, buf_len - chars_written, "valid_squitter_frames_by_source",
                            valid_squitter_frames_by_source, "%u", true);
            chars_written +=
                ArrayToJSON(buf + chars_written, buf_len - chars_written, "raw_extended_squitter_frames_by_source",
                            raw_extended_squitter_frames_by_source, "%u", true);
            chars_written +=
                ArrayToJSON(buf + chars_written, buf_len - chars_written, "valid_extended_squitter_frames_by_source",
                            valid_extended_squitter_frames_by_source, "%u", true);
            chars_written += ArrayToJSON(buf + strlen(buf), buf_len - strlen(buf), "demods_1090_by_source",
                                         demods_1090_by_source, "%u",
                                         false);  // No trailing comma.
            chars_written += snprintf(buf + chars_written, buf_len - chars_written, "}");
            return chars_written;
        }
    };

    /**
     * Default constructor. Uses default config values.
     */
    AircraftDictionary() 
    {
        // Избегайте перераспределения хэш-карты, чтобы предотвратить фрагментацию.
        dict.max_load_factor(1.0);
        dict.reserve(kMaxNumAircraft);
    };

    /**
     * Constructor with config values specified.
     */
    AircraftDictionary(AircraftDictionaryConfig_t config_in) : config_(config_in) {};

    /**
    * Удаляет все самолёты из словаря самолётов.
    */
    void Init();

    /**
    * Удаляет устаревшие самолёты из словаря.
    * @param[in] timestamp_us Текущая временная метка в микросекундах, используемая для удаления. Самолёты старше timestamp_us
    * минус интервал удаления будут удалены.
    */
    void Update(uint32_t timestamp_us);

    /**
    * Зарегистрировать попытку демодуляции на частоте 1090 МГц. Используется для записи статистики производительности. Обратите внимание, что приращение не будет
    * видно до следующего обновления словаря.
    */
    inline void Record1090Demod(int16_t source = -1) 
    {
        metrics_counter_.demods_1090++;
        if (source >= 0 && source < kMaxNumSources) 
        {
            metrics_counter_.demods_1090_by_source[source]++;
        }
    }

    /**
    * Регистрирует полученный 56-битный сквиттер-кадр. Используется для записи статистики производительности. Корректные кадры автоматически
    * записываются во время приёма. Эта функция разбивается на части, чтобы необработанные кадры без попыток приёма
    * могли быть записаны. Обратите внимание, что приращение не будет видно до следующего обновления словаря.
    */
    inline void Record1090RawSquitterFrame(int16_t source = -1) 
    {
        metrics_counter_.raw_squitter_frames++;
        if (source > 0) 
        {
            metrics_counter_.raw_squitter_frames_by_source[source]++;
        }
    }

    /**
    * Регистрирует полученный 112-битный расширенный сквиттер-кадр. Используется для записи статистики производительности. Корректные кадры
    * автоматически записываются во время приёма. Эта функция разбивается на части, так что необработанные кадры без попыток приёма
    * могут быть записаны. Обратите внимание, что приращение не будет видно до следующего обновления словаря.
    */
    inline void Record1090RawExtendedSquitterFrame(int16_t source = -1) 
    {
        metrics_counter_.raw_extended_squitter_frames++;
        if (source > 0) {
            metrics_counter_.raw_extended_squitter_frames_by_source[source]++;
        }
    }
    /**
    * Принимает пакет Decoded1090Packet и использует его для вставки и обновления данных соответствующего воздушного судна.
    * @param[in] Пакет Decoded1090Packet для приёма. Может быть 56-битным (сквиттер) или 112-битным (расширенный сквиттер).
    * Передаётся как ссылка, поскольку эта функция может помечать пакеты как допустимые.
    * @retval True в случае успеха, false в случае сбоя.
    */
    bool IngestDecoded1090Packet(Decoded1090Packet &packet);

    /**
    * Принимает пакет ответа Identity Surveillance Reply и использует его для обновления данных о соответствующем воздушном судне. Доступно для
    * тестирования, но обычно вызывается функцией IngestDecoded1090Packet.
    * Примечание: эта функция требует, чтобы пакет был помечен как действительный с помощью функции ForceValid(). Если пакет
    * действителен и не соответствует коду ICAO в словаре воздушных судов, будет добавлен новый самолет.
    * @param[in] пакет IdentityReplyPacket для приема.
    * @retval True в случае успеха, false в случае сбоя.
    */
    bool IngestIdentityReplyPacket(IdentityReplyPacket packet);

    /**
    * Принимает пакет ответа Altitude Surveillance Reply и использует его для обновления данных соответствующего воздушного судна. Предоставляется для
    * тестирования, но обычно вызывается функцией IngestDecoded1090Packet.
    * Примечание: эта функция требует, чтобы пакет был помечен как действительный с помощью функции ForceValid(). Если пакет
    * действителен и не соответствует коду ICAO в словаре воздушных судов, будет добавлен новый самолет.
    * @param[in] пакет AltitudeReplyPacket для приема.
    * @retval True в случае успеха, false в случае возникновения неполадки.
    */
    bool IngestAltitudeReplyPacket(AltitudeReplyPacket packet);

    /**
    * Принимает пакет ответа на все вызовы и использует его для обновления данных соответствующего воздушного судна. Доступно для тестирования, но обычно
    * вызывается функцией IngestDecoded1090Packet.
    *
    * В настоящее время мы принимаем только все пакеты ответа на вызовы с идентификатором запросчика 0 (ответы на спонтанные
    * сигналы обнаружения), поскольку у нас нет способа узнать идентификатор запросчика наземных станций наблюдения.
    */
    bool IngestAllCallReplyPacket(AllCallReplyPacket packet);

    //===============================================================
    int findAircraft(uint32_t icao);
    int findFreeSlot();
    void cleanOldAircrafts();
    void transmitAllAircrafts();
    void transmitBaseAllAircrafts();

    //===============================================================




    /**
    * Захватывает ADSBPacket напрямую. Доступно для тестирования, но обычно вызывается
    * IngestDecoded1090Packet и не должно использоваться напрямую.
    * @param[in] Пакет ADSBPacket для захвата. Производный от Decoded1090Packet с DF=17-19.
    * @retval True в случае успеха, false в случае сбоя.
    */
    bool IngestADSBPacket(ADSBPacket packet);

    /**
     * Returns the number of aircraft currently in the dictionary.
     * @retval Number of aircraaft that are currently in the dictionary.
     */
    uint16_t GetNumAircraft();

    /**
    * Добавляет объект "Воздушное судно" в словарь воздушных судов, хешированный по адресу ICAO.
    * @param[in] aircraft Самолет для вставки.
    * @retval True, если вставка прошла успешно, false в случае неудачи.
    */
    bool InsertAircraft(const Aircraft1090 &aircraft);

    /**
    * Удалить воздушное судно из словаря по адресу ICAO.
    * @param[in] icao_address — адрес ICAO воздушного судна, которое необходимо удалить из словаря.
    * @retval True, если удаление успешно, false, если воздушное судно не найдено.
    */
    bool RemoveAircraft(uint32_t icao_address);

    /**
    * Извлечь информацию о воздушном судне из словаря.
    * @param[in] icao_address Адрес для поиска воздушного судна.
    * @param[out] aircraft_out Ссылка на воздушное судно, куда следует поместить найденное воздушное судно в случае успеха.
    * @retval True, если воздушное судно было найдено и извлечено, false, если воздушного судна нет в словаре.
    */
    bool GetAircraft(uint32_t icao_address, Aircraft1090 &aircraft_out) const;

    /**
    * Проверяет, есть ли самолёт в словаре.
    * @param[in] icao_address Адрес для поиска самолёта.
    * @retval True, если самолёт есть в словаре, false — если нет.
    */
    bool ContainsAircraft(uint32_t icao_address) const;

    /**
    * Возвращает указатель на воздушное судно, если оно есть в словаре воздушных судов.
    * @param[in] icao_address Адрес ICAO искомого воздушного судна.
    * @retval Указатель на воздушное судно, если оно есть, или NULL, если его нет в словаре.
    */
    Aircraft1090 *GetAircraftPtr(uint32_t icao_address);

    /**
     * Используется для включения или отключения фильтра положения СЛР.
     * @param[in] enabled Значение True для включения фильтра, значение false для его отключения.
     */
    inline void SetCPRPositionFilterEnabled(bool enabled) { config_.enable_cpr_position_filter = enabled; }

    /**
 * Проверяем, включён ли фильтр положения  CPR.
 * @retval True, если фильтр включён, false, если он выключен.
 */
    inline bool CPRPositionFilterIsEnabled() { return config_.enable_cpr_position_filter; }

    std::unordered_map<uint32_t, Aircraft1090> dict;  // index Aircraft objects by their ICAO identifier

    Metrics metrics;

   private:

    // Вспомогательные функции для приема определенных типов пакетов ADS-B, вызываемые функцией IngestADSBPacket.

    /**
    * ОБЩИЙ КОММЕНТАРИЙ ДЛЯ ВСЕХ ПОМОЩНИКОВ ПРИЕМА СООБЩЕНИЙ
    * Принимает сообщение ADS-B типа <Тип сообщения>. Вызывается функцией IngestADSBPacket, которая проверяет,
    * что пакет является корректным и имеет правильный формат нисходящего канала.
    * @param[out] aircraft Ссылка на тип воздушного судна для заполнения информацией, извлеченной из пакета.
    * @param[in] ADSBPacket для приема.
    * @retval True, если сообщение было принято успешно, false в противном случае.
    */

    bool ApplyAircraftIDMessage(Aircraft1090 &aircraft, ADSBPacket packet);
    bool ApplySurfacePositionMessage(Aircraft1090 &aircraft, ADSBPacket packet);
    bool ApplyAirbornePositionMessage(Aircraft1090 &aircraft, ADSBPacket packet);
    bool ApplyAirborneVelocitiesMessage(Aircraft1090 &aircraft, ADSBPacket packet);
    bool ApplyAircraftStatusMessage(Aircraft1090 &aircraft, ADSBPacket packet);
    bool ApplyTargetStateAndStatusInfoMessage(Aircraft1090 &aircraft, ADSBPacket packet);
    bool ApplyAircraftOperationStatusMessage(Aircraft1090 &aircraft, ADSBPacket packet);

    AircraftDictionaryConfig_t config_;
    // Счётчики в metrics_counter_ увеличиваются, затем metrics_counter_ заменяется на metrics во время обновления словаря.
    // Это гарантирует, что общедоступная структура metrics всегда содержит корректные данные.
    Metrics metrics_counter_;
};

#endif /* AIRCRAFT_DICTIONARY_HH_ */