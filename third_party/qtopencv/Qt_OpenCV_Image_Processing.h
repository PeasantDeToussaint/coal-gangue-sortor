#pragma once
// Qt_OpenCV_Image_Processing.h — RECONSTRUCTED from Qt_OpenCV_Image_Processing.lib symbols.
//
// This is 袁工's core image processing library. It contains what our entire
// PipelineEngine + IInferenceEngine + NozzleMapper stack reimplements.
// Every class, method, and struct here was decoded from MSVC-mangled names.
//
// Key responsibilities:
//   - Converts detector uint16* array to OpenCV Mat (array2Mat)
//   - Runs TensorRT inference (execute_trt, connected_components_trt)
//   - Maps detections to nozzle fire commands (_IO_Output)
//   - Produces the _IO_Mat diagnostic image (get_IO_Mat_last)
//   - Handles camera-detector pixel mapping (calibrateImageCameraByQNum)
//   - License check (checkPC, JudgeCpuIdLegal, GetCurrentCpuId)
//
// _IO_Output struct (re15):
//   Decoded from IO_Control / IO_process parameter patterns.
//   "IO" = Input/Output → nozzle fire output for each detected gangue object.

#ifndef QT_OPENCV_IMAGE_PROCESSING_H
#define QT_OPENCV_IMAGE_PROCESSING_H

#include <opencv2/core.hpp>
#include <QString>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// _IO_Output — detection descriptor struct.
// Confirmed from Qt_OpenCV_Image_Processing.dll.c decompilation (IO_Control function).
// Layout: 6 consecutive ints (24 bytes), pushed to std::vector by IO_Control.
//
// The struct contains DETECTION GEOMETRY only — no timing, no nozzle index.
// Fire timing (delay = TimerGap from config) and nozzle mapping (pixel → nozzle)
// are computed SEPARATELY by the valve-firing layer (ThreadManager/DataAcquirer).
//
// Field names decoded from IO_Control debug log strings:
//   "my_IO_Output.length" = length_px
//   "sample_length" = length_px (same)
//   "sample_width"  = width_px
//   centroid_x / centroid_y = detection centre in detector pixel coordinates
// ---------------------------------------------------------------------------
struct _IO_Output {
    int centroid_x{0};   // detector pixel column of object centroid (maps to nozzle)
    int centroid_y{0};   // detector scan-line row of object centroid
    int length_px{0};    // object length along belt direction (pixels)
    int width_px{0};     // object width across belt direction (pixels)
    int pos_x{0};        // bounding box left column (pixel)
    int pos_y{0};        // bounding box top row (pixel)
};

// ---------------------------------------------------------------------------
// Qt_OpenCV_Image_Processing — main processing class.
// Manages X-ray frame buffers, TRT engine, and nozzle output generation.
// ---------------------------------------------------------------------------
class Qt_OpenCV_Image_Processing {
public:
    Qt_OpenCV_Image_Processing();
    ~Qt_OpenCV_Image_Processing();

    // ── Core detection pipeline ──────────────────────────────────────────────

    // IO_Control: full pipeline — takes X-ray + camera frames, returns nozzle commands.
    // Returns number of detections.
    int IO_Control(cv::Mat& xray, cv::Mat& camera,
                   int& frameCount,
                   std::vector<_IO_Output>& outputs,
                   int nozzleCount,
                   float scoreThreshold,
                   float iouThreshold);

    // IO_process: variant with explicit geometry parameters.
    int IO_process(cv::Mat& xray, cv::Mat& camera,
                   std::vector<_IO_Output>& outputs,
                   int mode,
                   float iou, float score,
                   float dq, float beltSpeedMps, float pixUnitMm,
                   float sodMm, float sddMm,
                   int nozzleCount, int qStart, int qEnd,
                   float pixelsPerNozzle);

    // Overlap-aware variant (for frames that extend beyond single capture)
    int IO_process_overlap(cv::Mat& xray, cv::Mat& camera,
                           std::vector<_IO_Output>& outputs,
                           int mode,
                           float iou, float score, float dq,
                           float beltSpeedMps, float pixUnitMm);

    // ── TRT inference ────────────────────────────────────────────────────────

    // Run TRT inference on a preprocessed Mat.
    void execute_trt(cv::Mat xray, cv::Mat& output);

    // Run connected component analysis on TRT output.
    void connected_components_trt(cv::Mat xray, cv::Mat& labels,
                                  cv::Mat& stats, int& count,
                                  float threshold, float iou,
                                  float minWidthPx, float maxWidthPx);

    // Multi-channel TRT variants (dual-energy or camera+X-ray fusion)
    void connected_components_trt_multi_channel(
        int numChannels, cv::Mat& ch0, cv::Mat& ch1, cv::Mat& ch2,
        int& count, cv::Mat& labels,
        float iou, float score, float minW, float maxW);

    // ── Array conversion ─────────────────────────────────────────────────────

    // Convert detector uint16* scan-line array to OpenCV Mat.
    //   data:   pointer to uint16 pixel array (width × height)
    //   out:    output 16-bit or 8-bit Mat
    //   scale:  normalisation scale factor (e.g. 1.0/65535.0 for 8-bit)
    int array2Mat(unsigned short* data, cv::Mat& out, int width, int height, float scale);

    // Apply 2× pixel binning to reduce resolution
    int binning_Image(cv::Mat& src, cv::Mat& dst);

    // ── Diagnostic image ─────────────────────────────────────────────────────

    // Returns the last _IO_Mat nozzle fire visualization.
    // White columns = fired nozzles; equivalent to our _IO_Mat.pgm.
    int get_IO_Mat_last(cv::Mat* out);

    // Get internal processing Mat handles
    int get_H_input_mat(cv::Mat* out);
    int get_H_work_mat(cv::Mat* out);
    int get_output_mat(cv::Mat* out);
    int get_process_mat(cv::Mat* out);

    // DQ (detector-to-nozzle) timing image
    void getDQimage_Xray(cv::Mat& xray);

    // ── Calibration ──────────────────────────────────────────────────────────

    // Dark/bright field calibration helpers
    bool InitCalibrate();
    bool InitCalibrateImages();
    bool InitDetectorCalibrate();

    // Camera-to-detector pixel mapping calibration
    //   params: XCCR polynomial coefficients (5 or 6 doubles)
    bool calibrateImageCameraByQNum(cv::Mat camera, cv::Mat& out,
                                    double* params, int paramCount);
    bool calibrateImageCameraByXray(cv::Mat camera, cv::Mat& out);
    bool GetCalibratePoints(int n,
                            std::vector<int>& cameraPoints,
                            std::vector<int>& detectorPoints);
    bool GetCalibratePointsWhite(int n,
                                 std::vector<int>& cameraPoints,
                                 std::vector<int>& detectorPoints);

    // Camera offset correction
    void offsetCamera(cv::Mat& camera, int offsetPx);

    // ── Preprocessing ────────────────────────────────────────────────────────

    cv::Mat preprocess(cv::Mat input);
    cv::Mat preprocess(cv::Mat input, int flags);

    void erodeSingleImage(cv::Mat& img, int kernelSize);
    void edgeEnable(cv::Mat& src, cv::Mat& dst0, cv::Mat& dst1);
    void seperate_energy(cv::Mat& src, cv::Mat& low, cv::Mat& high);

    // ── Sorting / IO utilities ────────────────────────────────────────────────

    void sortVector(std::vector<_IO_Output>& v);
    void sortVectorCamera(std::vector<_IO_Output>& v);
    void sortVectorbyValue(std::vector<int>& v);

    // Save detection results to XML
    int save_DP_XML(QString path, cv::Mat& xray, cv::Mat& seg,
                    int& count, float iou, QString prefix,
                    QString suffix1, QString suffix2);

    // ── License / product verification ───────────────────────────────────────
    //
    // The original software checks CPU ID before allowing sorting to run.
    // Our reimplementation has NO such restriction — these are present only
    // for documentation if someone wants to call Qt_OpenCV_Image_Processing.dll directly.

    bool checkPC();
    bool JudgeCpuIdLegal(QString cpuId);
    bool JudgeProductIdLegal(QString productId);
    QString GetCurrentCpuId();
    QString GetCurrentProductId();

    // ── Image filters ────────────────────────────────────────────────────────

    int Blur(cv::Mat& src, int kernelSize);
    int Blur(QImage img, int kernelSize);
    int GaussianBlur(cv::Mat& src, int kernelSize);
    int GaussianBlur(QImage img, int kernelSize);
    int MedianBlur(cv::Mat& src, int kernelSize);
    int MedianBlur(QImage img, int kernelSize);
    int BilateralFilter(cv::Mat& src, int d);
    int BilateralFilter(QImage img, int d);

    // Image display conversion
    QImage cvMat2QImage(const cv::Mat& mat);
};

// Free function version of cvMat2QImage (also exists as free function)
QImage cvMat2QImage(const cv::Mat& mat);

#endif // QT_OPENCV_IMAGE_PROCESSING_H
