/* ============================================================================
   MAXIM REFERENCE ALGORITHM - FIXED VERSION
   ============================================================================ */

#include <stdint.h>
#include <stdbool.h>

// Algorithm Configuration
#define FREQ_SAMPLE         100     // Sampling frequency in Hz
#define BUFFER_SIZE         100     // 1 second at 100Hz
#define MA4_SIZE            4       // Moving average window size

// SpO2 lookup table from Maxim (R-ratio to SpO2%)
static const uint8_t uch_spo2_table[184] = {
    95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
    99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
    97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
    90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
    80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
    66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
    28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
    3, 2, 1
};

// Working buffers
static int32_t an_x[BUFFER_SIZE];  // IR samples
static int32_t an_y[BUFFER_SIZE];  // RED samples

// Helper function prototypes
static void maxim_find_peaks(int32_t *pn_locs, int32_t *n_npks, int32_t *pn_x,
                             int32_t n_size, int32_t n_min_height,
                             int32_t n_min_distance, int32_t n_max_num);
static void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *n_npks,
                                         int32_t *pn_x, int32_t n_size,
                                         int32_t n_min_height);
static void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks,
                                     int32_t *pn_x, int32_t n_min_distance);
static void maxim_sort_ascend(int32_t *pn_x, int32_t n_size);
static void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size);
static int32_t min(int32_t a, int32_t b) { return (a < b) ? a : b; }

/**
 * @brief Calculate heart rate and SpO2 using Maxim's algorithm
 * @param pun_ir_buffer: IR LED data buffer (100 samples)
 * @param n_ir_buffer_length: Buffer length (should be 100)
 * @param pun_red_buffer: Red LED data buffer (100 samples)
 * @param pn_spo2: Output SpO2 value (70-100%)
 * @param pch_spo2_valid: Output SpO2 validity flag (1=valid, 0=invalid)
 * @param pn_heart_rate: Output heart rate (40-200 bpm)
 * @param pch_hr_valid: Output HR validity flag (1=valid, 0=invalid)
 */
void maxim_heart_rate_and_oxygen_saturation(
    uint32_t *pun_ir_buffer,
    int32_t n_ir_buffer_length,
    uint32_t *pun_red_buffer,
    int32_t *pn_spo2,
    int8_t *pch_spo2_valid,
    int32_t *pn_heart_rate,
    int8_t *pch_hr_valid)
{
    uint32_t un_ir_mean;
    int32_t k, n_i_ratio_count;
    int32_t i, n_exact_ir_valley_locs_count, n_middle_idx;
    int32_t n_th1, n_npks;
    int32_t an_ir_valley_locs[15];
    int32_t n_peak_interval_sum;

    int32_t n_y_ac, n_x_ac;
    int32_t n_spo2_calc;
    int32_t n_y_dc_max, n_x_dc_max;
    int32_t n_y_dc_max_idx = 0;
    int32_t n_x_dc_max_idx = 0;
    int32_t an_ratio[5], n_ratio_average;
    int32_t n_nume, n_denom;

    // Initialize outputs to invalid
    *pn_heart_rate = -999;
    *pch_hr_valid = 0;
    *pn_spo2 = -999;
    *pch_spo2_valid = 0;

    // Calculate DC mean and remove DC from IR
    un_ir_mean = 0;
    for (k = 0; k < n_ir_buffer_length; k++) {
        un_ir_mean += pun_ir_buffer[k];
    }
    un_ir_mean = un_ir_mean / n_ir_buffer_length;

    // Remove DC and invert signal (so peak detector works as valley detector)
    for (k = 0; k < n_ir_buffer_length; k++) {
        an_x[k] = -1 * (pun_ir_buffer[k] - un_ir_mean);
    }

    // 4-point Moving Average filter
    for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++) {
        an_x[k] = (an_x[k] + an_x[k+1] + an_x[k+2] + an_x[k+3]) / 4;
    }

    // Calculate threshold for peak detection
    n_th1 = 0;
    for (k = 0; k < BUFFER_SIZE; k++) {
        n_th1 += an_x[k];
    }
    n_th1 = n_th1 / BUFFER_SIZE;

    // FIXED: Correct threshold clamping logic
    if (n_th1 < 30) n_th1 = 30;   // Minimum threshold
    if (n_th1 > 60) n_th1 = 60;   // Maximum threshold

    // Initialize valley locations
    for (k = 0; k < 15; k++) {
        an_ir_valley_locs[k] = 0;
    }

    // FIXED: Increased minimum distance to prevent false peaks
    // For 100 Hz sampling:
    // - Max HR = 200 BPM = 3.33 Hz = 30 samples between peaks
    // - Min HR = 40 BPM = 0.67 Hz = 150 samples between peaks
    // Using min_distance = 25 allows up to 240 BPM while rejecting noise
    maxim_find_peaks(an_ir_valley_locs, &n_npks, an_x, BUFFER_SIZE, n_th1, 25, 15);

    // Calculate heart rate from peak intervals
    n_peak_interval_sum = 0;
    if (n_npks >= 2) {
        for (k = 1; k < n_npks; k++) {
            n_peak_interval_sum += (an_ir_valley_locs[k] - an_ir_valley_locs[k-1]);
        }
        n_peak_interval_sum = n_peak_interval_sum / (n_npks - 1);
        *pn_heart_rate = (int32_t)((FREQ_SAMPLE * 60) / n_peak_interval_sum);

        // Validate heart rate is in physiological range (40-200 BPM)
        if (*pn_heart_rate >= 40 && *pn_heart_rate <= 200) {
            *pch_hr_valid = 1;
        } else {
            *pn_heart_rate = -999;
            *pch_hr_valid = 0;
        }
    }

    // Load raw values for SpO2 calculation
    for (k = 0; k < n_ir_buffer_length; k++) {
        an_x[k] = pun_ir_buffer[k];
        an_y[k] = pun_red_buffer[k];
    }

    n_exact_ir_valley_locs_count = n_npks;

    // Calculate R-ratio using AC/DC components
    n_ratio_average = 0;
    n_i_ratio_count = 0;
    for (k = 0; k < 5; k++) an_ratio[k] = 0;

    // Validate valley locations
    for (k = 0; k < n_exact_ir_valley_locs_count; k++) {
        if (an_ir_valley_locs[k] > BUFFER_SIZE) {
            *pn_spo2 = -999;
            *pch_spo2_valid = 0;
            return;
        }
    }

    // Find max between valley locations and calculate R-ratio
    for (k = 0; k < n_exact_ir_valley_locs_count - 1; k++) {
        n_y_dc_max = -16777216;
        n_x_dc_max = -16777216;

        if (an_ir_valley_locs[k+1] - an_ir_valley_locs[k] > 3) {
            // Find DC max (peak) between valleys
            for (i = an_ir_valley_locs[k]; i < an_ir_valley_locs[k+1]; i++) {
                if (an_x[i] > n_x_dc_max) {
                    n_x_dc_max = an_x[i];
                    n_x_dc_max_idx = i;
                }
                if (an_y[i] > n_y_dc_max) {
                    n_y_dc_max = an_y[i];
                    n_y_dc_max_idx = i;
                }
            }

            // Calculate AC component (linear interpolation)
            n_y_ac = (an_y[an_ir_valley_locs[k+1]] - an_y[an_ir_valley_locs[k]]) *
                     (n_y_dc_max_idx - an_ir_valley_locs[k]);
            n_y_ac = an_y[an_ir_valley_locs[k]] +
                     n_y_ac / (an_ir_valley_locs[k+1] - an_ir_valley_locs[k]);
            n_y_ac = an_y[n_y_dc_max_idx] - n_y_ac;  // RED AC

            n_x_ac = (an_x[an_ir_valley_locs[k+1]] - an_x[an_ir_valley_locs[k]]) *
                     (n_x_dc_max_idx - an_ir_valley_locs[k]);
            n_x_ac = an_x[an_ir_valley_locs[k]] +
                     n_x_ac / (an_ir_valley_locs[k+1] - an_ir_valley_locs[k]);
            n_x_ac = an_x[n_y_dc_max_idx] - n_x_ac;  // IR AC

            // Calculate R = (RED_AC/RED_DC) / (IR_AC/IR_DC)
            n_nume = (n_y_ac * n_x_dc_max) >> 7;  // Prepare X100 to preserve precision
            n_denom = (n_x_ac * n_y_dc_max) >> 7;

            if (n_denom > 0 && n_i_ratio_count < 5 && n_nume != 0) {
                an_ratio[n_i_ratio_count] = (n_nume * 100) / n_denom;
                n_i_ratio_count++;
            }
        }
    }

    // Use median R-ratio value
    maxim_sort_ascend(an_ratio, n_i_ratio_count);
    n_middle_idx = n_i_ratio_count / 2;

    if (n_middle_idx > 1) {
        n_ratio_average = (an_ratio[n_middle_idx-1] + an_ratio[n_middle_idx]) / 2;
    } else {
        n_ratio_average = an_ratio[n_middle_idx];
    }

    // Convert R-ratio to SpO2 using lookup table
    if (n_ratio_average > 2 && n_ratio_average < 184) {
        n_spo2_calc = uch_spo2_table[n_ratio_average];
        *pn_spo2 = n_spo2_calc;
        *pch_spo2_valid = 1;
    } else {
        *pn_spo2 = -999;
        *pch_spo2_valid = 0;
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

static void maxim_find_peaks(int32_t *pn_locs, int32_t *n_npks, int32_t *pn_x,
                             int32_t n_size, int32_t n_min_height,
                             int32_t n_min_distance, int32_t n_max_num)
{
    maxim_peaks_above_min_height(pn_locs, n_npks, pn_x, n_size, n_min_height);
    maxim_remove_close_peaks(pn_locs, n_npks, pn_x, n_min_distance);
    *n_npks = min(*n_npks, n_max_num);
}

static void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *n_npks,
                                         int32_t *pn_x, int32_t n_size,
                                         int32_t n_min_height)
{
    int32_t i = 1, n_width;
    *n_npks = 0;

    while (i < n_size - 1) {
        if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i-1]) {
            n_width = 1;
            while (i + n_width < n_size && pn_x[i] == pn_x[i+n_width]) {
                n_width++;
            }
            if (pn_x[i] > pn_x[i+n_width] && (*n_npks) < 15) {
                pn_locs[(*n_npks)++] = i;
                i += n_width + 1;
            } else {
                i += n_width;
            }
        } else {
            i++;
        }
    }
}

static void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks,
                                     int32_t *pn_x, int32_t n_min_distance)
{
    int32_t i, j, n_old_npks, n_dist;

    maxim_sort_indices_descend(pn_x, pn_locs, *pn_npks);

    for (i = -1; i < *pn_npks; i++) {
        n_old_npks = *pn_npks;
        *pn_npks = i + 1;
        for (j = i + 1; j < n_old_npks; j++) {
            n_dist = pn_locs[j] - (i == -1 ? -1 : pn_locs[i]);
            if (n_dist > n_min_distance || n_dist < -n_min_distance) {
                pn_locs[(*pn_npks)++] = pn_locs[j];
            }
        }
    }

    maxim_sort_ascend(pn_locs, *pn_npks);
}

static void maxim_sort_ascend(int32_t *pn_x, int32_t n_size)
{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_x[i];
        for (j = i; j > 0 && n_temp < pn_x[j-1]; j--) {
            pn_x[j] = pn_x[j-1];
        }
        pn_x[j] = n_temp;
    }
}

static void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size)
{
    int32_t i, j, n_temp;
    for (i = 1; i < n_size; i++) {
        n_temp = pn_indx[i];
        for (j = i; j > 0 && pn_x[n_temp] > pn_x[pn_indx[j-1]]; j--) {
            pn_indx[j] = pn_indx[j-1];
        }
        pn_indx[j] = n_temp;
    }
}
