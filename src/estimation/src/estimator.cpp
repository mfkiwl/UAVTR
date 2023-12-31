#include "estimator.hpp"
#include <Eigen/Sparse>
#include <cassert>
#include <chrono>
#include <fstream>
#include <random>
#include <vector>

struct StateUpdateFreq {
  uint64_t count;
  long t0; // milliseconds
  long t1; // milliseconds
};
auto state_update_freq_map = std::map<std::string, StateUpdateFreq>{};

void record_state_update(const std::string &name) {
  auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
  if (state_update_freq_map.find(name) == state_update_freq_map.end()) {
    state_update_freq_map[name] = StateUpdateFreq{0, time, -1};
  }
  state_update_freq_map[name].count++;
  state_update_freq_map[name].t1 = time;
}

Estimator::Estimator(EstimatorConfig config) : config_(config) {
  const double dt = 1.0 / 128.0;
  Eigen::MatrixXf A(12, 12);
  get_A(A, dt);

  Eigen::MatrixXf Q = Eigen::MatrixXf::Zero(12, 12);
  Eigen::Matrix3f acc_variance;
  acc_variance << 3.182539, 0, 0,
      0, 3.187015, 0,
      0, 0, 1.540428;
  auto Q33 = Eigen::MatrixXf::Identity(3, 3) * std::pow(dt, 4) / 4.0;
  auto Q36 = Eigen::MatrixXf::Identity(3, 3) * std::pow(dt, 3) / 2.0;
  auto Q39 = Eigen::MatrixXf::Identity(3, 3) * std::pow(dt, 2) / 2.0;
  auto Q66 = Eigen::MatrixXf::Identity(3, 3) * std::pow(dt, 2);
  auto Q69 = Eigen::MatrixXf::Identity(3, 3) * dt;
  Q.block(0, 0, 3, 3) = Q33 * acc_variance;
  Q.block(0, 3, 3, 3) = Q36 * acc_variance;
  Q.block(3, 0, 3, 3) = Q36 * acc_variance;
  Q.block(0, 6, 3, 3) = Q39 * acc_variance;
  Q.block(6, 0, 3, 3) = Q39 * acc_variance;
  Q.block(3, 3, 3, 3) = Q66 * acc_variance;
  Q.block(3, 6, 3, 3) = Q69 * acc_variance;
  Q.block(6, 3, 3, 3) = Q69 * acc_variance;
  Q.block(6, 6, 3, 3) = Eigen::MatrixXf::Identity(3, 3) * acc_variance;
  Q *= 4;

  // relative position measurement
  Eigen::MatrixXf C(2, 12);
  C << 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;
  Eigen::MatrixXf R(2, 2);
  R = Eigen::MatrixXf::Identity(2, 2) * 1.0;

  Eigen::MatrixXf P(12, 12);
  P = Eigen::MatrixXf::Identity(12, 12) * 10000.0;

  kf_ = std::make_unique<KalmanFilter>(A, C, Q, R, P);

  const std::array<float, 3> a = {1.0, -1.56101808, 0.64135154};
  const std::array<float, 3> b = {0.02008337, 0.04016673, 0.02008337};
  // Create a low pass filter objects.
  for (int i = 0; i < 3; i++)
    lp_acc_filter_arr_[i] = std::make_unique<LowPassFilter<float, 3>>(b, a);

  optflow_ = cv::DISOpticalFlow::create(2);
}

Estimator::~Estimator() {
  // log out the frequency of state updates to a file
  std::ofstream file("/tmp/state_update_freq.txt");

  for (const auto &pair : state_update_freq_map) {
    const auto &name = pair.first;
    const auto &freq = pair.second;
    auto dt = static_cast<double>(freq.t1 - freq.t0);
    auto freq_hz = static_cast<double>(freq.count) / dt * 1000.0;
    file << name << " " << freq_hz << " Hz" << std::endl;
  }

  file.close();
}

void Estimator::get_A(Eigen::MatrixXf &A, double dt) {
  A.setZero();
  // incorporate IMU after tests
  auto ddt2 = static_cast<float>(dt * dt * .5);
  float mult = 1.0;
  assert(dt > 0 && dt < 1);
  A << 1, 0, 0, dt, 0, 0, ddt2, 0, 0, -ddt2 * mult, 0, 0,
      0, 1, 0, 0, dt, 0, 0, ddt2, 0, 0, -ddt2 * mult, 0,
      0, 0, 1, 0, 0, dt, 0, 0, ddt2, 0, 0, -ddt2 * mult,
      0, 0, 0, 1, 0, 0, dt, 0, 0, -dt * mult, 0, 0,
      0, 0, 0, 0, 1, 0, 0, dt, 0, 0, -dt * mult, 0,
      0, 0, 0, 0, 0, 1, 0, 0, dt, 0, 0, -dt * mult,
      0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1;
}

Eigen::Vector3f Estimator::update_target_position(
    const Eigen::Vector2f &bbox_c, const EigenAffine &cam_T_enu,
    const Eigen::Matrix3f &K, const Eigen::Vector3f &t) {
  float height = get_height();
  if (height < 1.0f)
    return {0, 0, 0};

  Eigen::Vector3f Pt = target_position(bbox_c, cam_T_enu, K, height);

  if (kf_->is_initialized()) {
    Eigen::Vector2f xy_meas(2);
    xy_meas << Pt[0], Pt[1];
    kf_->update(xy_meas);
    record_state_update(__FUNCTION__);
  } else {
    Eigen::VectorXf x0(12);
    x0 << Pt[0], Pt[1], Pt[2], 0, 0, 0, 0, 0, 0, 0, 0, 0;
    kf_->init(x0);
  }

  return Pt;
}

void Estimator::update_height(const float height) {
  latest_height_.store(height);

  if (!kf_->is_initialized())
    return;

  static Eigen::MatrixXf C_height(1, 12);
  C_height << 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0;
  static Eigen::MatrixXf R(1, 1);
  R << 3.3224130e+00; // height measurement noise

  Eigen::VectorXf h(1);
  h << -height;
  // the relative height is negative
  kf_->update(h, C_height, R);

  record_state_update(__FUNCTION__);
}

void Estimator::update_imu_accel(const Eigen::Vector3f &accel, double time) {
  if (!kf_->is_initialized())
    return;
  if (pre_imu_time_ < 0) {
    pre_imu_time_ = time;
    return;
  }
  auto dt = time - pre_imu_time_;
  assert(dt > 0);
  assert(dt < 1.0);
  pre_imu_time_ = time;

  static Eigen::MatrixXf C_accel(3, 12);
  C_accel << 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0;
  static Eigen::MatrixXf R_accel(3, 3);
  R_accel << Eigen::Matrix3f::Identity() * 5;
  kf_->update(accel, C_accel, R_accel);

  Eigen::MatrixXf A(12, 12);
  get_A(A, dt);
  kf_->predict(A);

  record_state_update(__FUNCTION__);
}

void Estimator::update_cam_imu_accel(const Eigen::Vector3f &accel, const Eigen::Vector3f &omega,
                                     const Eigen::Matrix3f &imu_R_enu, const Eigen::Vector3f &arm) {
  return;
  if (!kf_->is_initialized())
    return;

  Eigen::Vector3f accel_enu = imu_R_enu * accel;
  // subtract gravity
  accel_enu[2] -= 9.81;
  Eigen::Vector3f omega_enu = imu_R_enu * omega;

  Eigen::Vector3f accel_body = accel_enu - omega_enu.cross(omega_enu.cross(arm));

  static Eigen::MatrixXf C_accel(3, 12);
  C_accel << 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0;
  static Eigen::MatrixXf R_accel(3, 3);
  R_accel << Eigen::Matrix3f::Identity() * 4;
  kf_->update(accel_body, C_accel, R_accel);
}

void Estimator::visjac_p(const Eigen::MatrixXf &uv,
                         const Eigen::VectorXf &depth,
                         const Eigen::Matrix3f &K,
                         Eigen::MatrixXf &L) {
  assert(uv.cols() == depth.size());

  L.resize(depth.size() * 2, 6);
  L.setZero();
  const Eigen::Matrix3f Kinv = K.inverse();

  for (int i = 0; i < uv.cols(); i++) {
    const float z = depth(i);
    const Eigen::Vector3f p(uv(0, i), uv(1, i), 1.0);

    // convert to normalized image-plane coordinates
    const Eigen::Vector3f xy = Kinv * p;
    float x = xy(0);
    float y = xy(1);

    // 2x6 Jacobian for this point
    Eigen::Matrix<float, 2, 6> Lp;
    Lp << -1 / z, 0.0, x / z, x * y, -(1 + x * x), y,
        0.0, -1 / z, y / z, 1 + y * y, -x * y, -x;
    Lp = K.block(0, 0, 2, 2) * Lp;

    // push into Jacobian
    L.block(2 * i, 0, 2, 6) = Lp;
  }
}

void solve_sampled(const Eigen::MatrixXf &J,
                   const Eigen::VectorXf &flow_vectors,
                   Eigen::VectorXf &cam_vel_est) {
  // solve for velocity
  cam_vel_est = (J.transpose() * J).ldlt().solve(J.transpose() * flow_vectors);
}

bool Estimator::RANSAC_vel_regression(const Eigen::MatrixXf &J,
                                      const Eigen::VectorXf &flow_vectors,
                                      Eigen::VectorXf &cam_vel_est) {
  // cam_vel_est = (J.transpose() * J).ldlt().solve(J.transpose() * flow_vectors);
  // return true;

  // https://rpg.ifi.uzh.ch/docs/Visual_Odometry_Tutorial.pdf slide 68
  // >> outlier_percentage = .75
  // >>> np.log(1 - 0.999) / np.log(1 - (1 - outlier_percentage) ** n_samples)
  // 438.63339476983924
  const size_t n_iterations = 2000;
  const size_t n_samples{3}; // minimum required to fit model
  const size_t n_points = flow_vectors.rows() / 2;

  std::random_device rd;                                  // obtain a random number from hardware
  std::minstd_rand gen(rd());                             // seed the generator
  std::uniform_int_distribution<> distr(0, n_points - 1); // define the range

  auto best_inliers = std::vector<size_t>{}; // best inlier indices
  Eigen::MatrixXf J_samples(n_samples * 2, J.cols());
  J_samples.setZero();
  Eigen::VectorXf flow_samples(n_samples * 2);
  flow_samples.setZero();
  Eigen::VectorXf x_est(J.cols());
  std::vector<size_t> inlier_idxs;
  inlier_idxs.reserve(n_points);
  for (size_t iter{0}; iter <= n_iterations; ++iter) {
    // randomly select n_samples from data
    for (size_t i{0}; i < n_samples; ++i) {
      size_t idx = distr(gen);
      // take sampled data
      J_samples.block(i * 2, 0, 2, J.cols()) = J.block(idx * 2, 0, 2, J.cols());
      flow_samples.segment(i * 2, 2) = flow_vectors.segment(idx * 2, 2);
    }
    // solve for velocity
    x_est = (J_samples.transpose() * J_samples).ldlt().solve(J_samples.transpose() * flow_samples);

    Eigen::VectorXf error = J * x_est - flow_vectors;

    // compute inliers
    for (long i{0}; i < static_cast<long>(n_points); ++i) {
      const float error_x = error(i * 2);
      const float error_y = error(i * 2 + 1);
      const float error_norm = std::abs(error_x) + std::abs(error_y);
      if (std::isnan(error_norm) || std::isinf(error_norm)) {
        cam_vel_est = Eigen::VectorXf::Zero(J.cols());
        return false;
      }
      if (error_norm < config_.spatial_vel_flow_error) { // in pixels
        inlier_idxs.push_back(i);
      }
    }

    // bool is_omega_zero = x_est.segment(3, 3).norm() < 1e-1;
    bool is_omega_zero = true;
    if (best_inliers.size() < inlier_idxs.size() && is_omega_zero) {
      best_inliers = inlier_idxs;
    }

    if (static_cast<float>(best_inliers.size()) > 0.5f * static_cast<float>(n_points))
      break;

    inlier_idxs.clear();
  }

  std::cout << best_inliers.size() << " inliers out of " << n_points << std::endl;
  if (best_inliers.size() < static_cast<size_t>(static_cast<double>(n_points) * config_.flow_vel_rejection_perc)) {
    cam_vel_est = Eigen::VectorXf::Zero(J.cols());
    return false;
  }

  cam_vel_est = x_est;
  return true;

  J_samples.resize(best_inliers.size() * 2, J.cols());
  flow_samples.resize(best_inliers.size() * 2);

  // solve for best inliers
  for (size_t i{0}; i < best_inliers.size(); ++i) {
    J_samples.block(i * 2, 0, 2, J.cols()) = J.block(best_inliers[i] * 2, 0, 2, J.cols());
    flow_samples.segment(i * 2, 2) = flow_vectors.segment(best_inliers[i] * 2, 2);
  }
  cam_vel_est = (J_samples.transpose() * J_samples).ldlt().solve(J_samples.transpose() * flow_samples);
  return true;
}

void Estimator::store_flow_state(cv::Mat &frame, double time,
                                 const EigenAffine &cam_T_enu) {
  this->pre_frame_time_ = time;
  this->prev_frame_ = std::make_shared<cv::Mat>(frame);
  this->prev_cam_T_enu_ = cam_T_enu;
}

Eigen::Vector3f Estimator::update_flow_velocity(cv::Mat &frame, double time,
                                                const EigenAffine &base_T_odom,
                                                const EigenAffine &img_T_base,
                                                const Eigen::Matrix3f &K, const Eigen::Vector3f &omega,
                                                const Eigen::Vector3f &drone_omega) {
  cv::cvtColor(frame, frame, cv::COLOR_BGR2GRAY);
  EigenAffine cam_T_enu = base_T_odom * img_T_base;
  if (!prev_frame_) {
    store_flow_state(frame, time, cam_T_enu);
    return {0, 0, 0};
  }

  const double dt = time - pre_frame_time_;
  assert(dt > 0);
  if (dt > .2) {
    this->pre_frame_time_ = time;
    *prev_frame_ = frame;
    return {0, 0, 0};
  }

  static int count{0};
  static float height{0};
  count++;
  if (count > 1) {
    std::string time_str = std::to_string(time);
    cv::imwrite("/tmp/" + time_str + "_frame0.png", *prev_frame_);
    cv::imwrite("/tmp/" + time_str + "_frame1.png", frame);
    std::ofstream file("/tmp/" + time_str + "_flowinfo.txt");
    file << "time:" << time << std::endl;
    file << "prev_time:" << pre_frame_time_ << std::endl;
    file << "cam_R_enu:" << cam_T_enu.rotation() << std::endl;
    file << "height:" << get_height() << std::endl;
    file << "prev_height:" << height << std::endl;
    file << "r:" << img_T_base.translation() << std::endl;
    file << "K:" << std::endl
         << K << std::endl;
    file << "omega:" << omega << std::endl;
    file << "drone_omega:" << drone_omega << std::endl;
    file << "prev_R:" << prev_cam_T_enu_.rotation() << std::endl;
    file << "baseTodom:" << base_T_odom.matrix() << std::endl;
    file << "imgTbase:" << img_T_base.matrix() << std::endl;
  }
  height = get_height();

  // for (int i = 0; i < 3; i++) {
  //   if (std::abs(omega[i]) > 0.3 || std::abs(drone_omega[i]) > 0.3) {
  //     store_flow_state(frame, time, cam_T_enu);
  //     return {0, 0, 0};
  //   }
  // }

  cv::Mat flow;
  optflow_->calc(*prev_frame_, frame, flow);

  int every_nth = 16;
  std::vector<cv::Point2f> flow_vecs;
  flow_vecs.reserve(frame.rows * frame.cols / (every_nth * every_nth));
  std::vector<cv::Point> samples;
  samples.reserve(frame.rows * frame.cols / (every_nth * every_nth));
  // the multiplies are orientation dependant
  for (int row = (int) every_nth; row < (frame.rows - every_nth); row += every_nth) {
    for (int col = (int) every_nth; col < (frame.cols - every_nth); col += every_nth) {
      // Get the flow from `flow`, which is a 2-channel matrix
      const cv::Point2f &fxy = flow.at<cv::Point2f>(row, col);
      flow_vecs.push_back(fxy);
      samples.emplace_back(row, col);
    }
  }

  const float MAX_Z = 200;
  Eigen::VectorXf depth(samples.size());
  Eigen::MatrixXf uv = Eigen::MatrixXf(2, samples.size());
  Eigen::VectorXf flow_eigen(2 * flow_vecs.size());
  long insert_idx = 0;
  for (size_t i = 0; i < samples.size(); i++) {
    const bool is_flow_present = (flow_vecs[i].x != 0 && flow_vecs[i].y != 0);
    if (!is_flow_present)
      continue;

    const float Z = get_pixel_z_in_camera_frame(
        Eigen::Vector2f(samples[i].x, samples[i].y), prev_cam_T_enu_, K);
    if (Z < 0 || Z > MAX_Z || std::isnan(Z))
      continue;

    depth(insert_idx) = Z;
    uv(0, insert_idx) = static_cast<float>(samples[i].x);
    uv(1, insert_idx) = static_cast<float>(samples[i].y);
    flow_eigen(2 * insert_idx) = flow_vecs[i].x / dt;
    flow_eigen(2 * insert_idx + 1) = flow_vecs[i].y / dt;
    ++insert_idx;
  }
  if (insert_idx < 3) {
    store_flow_state(frame, time, cam_T_enu);
    return {0, 0, 0};
  }

  // resize the matrices to fit the filled values
  depth.conservativeResize(insert_idx);
  uv.conservativeResize(Eigen::NoChange, insert_idx);
  flow_eigen.conservativeResize(2 * insert_idx);

  Eigen::MatrixXf J; // Jacobian
  visjac_p(uv, depth, K, J);

  for (long i = 0; i < J.rows(); i++) {
    Eigen::Vector3f Jw = {J(i, 3), J(i, 4), J(i, 5)};
    flow_eigen(i) -= Jw.dot(omega);
    // Eigen::Vector3f Jv = {J(i, 0), J(i, 1), J(i, 2)};
    // flow_eigen(i) -= Jv.dot(drone_omega.cross(r));
  }
  Eigen::VectorXf cam_vel_est;
  bool success = RANSAC_vel_regression(J.block(0, 0, J.rows(), 3), flow_eigen, cam_vel_est);
  // bool success = RANSAC_vel_regression(J, flow_eigen, cam_vel_est);

  const Eigen::Vector3f v_base = img_T_base.rotation() * cam_vel_est.segment(0, 3)
      - drone_omega.cross(img_T_base.translation());
  const Eigen::Vector3f v_enu = base_T_odom.rotation() * v_base;

  if (success && kf_->is_initialized()) {
    static Eigen::MatrixXf C_vel(2, 12);
    C_vel << 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0;
    static Eigen::MatrixXf R_vel(2, 2);
    R_vel = Eigen::MatrixXf::Identity(2, 2) * 1.0;

    kf_->update(v_enu.segment(0, 2), C_vel, R_vel);

    record_state_update(__FUNCTION__);
  }

  store_flow_state(frame, time, cam_T_enu);
  return -v_enu;
}

float Estimator::get_pixel_z_in_camera_frame(
    const Eigen::Vector2f &pixel, const EigenAffine &cam_T_enu,
    const Eigen::Matrix3f &K, float height) const {
  if (height < 0)
    height = get_height();
  Eigen::Vector3f Pt = target_position(pixel, cam_T_enu, K, height);
  // transform back to camera frame
  Pt = cam_T_enu.inverse() * Pt;
  return Pt[2];
}

Eigen::Vector3f Estimator::target_position(const Eigen::Vector2f &pixel,
                                           const EigenAffine &cam_T_enu,
                                           const Eigen::Matrix3f &K, float height) const {
  Eigen::Matrix<float, 3, 3> Kinv = K.inverse();
  Eigen::Vector3f lr{0, 0, -1};
  Eigen::Vector3f Puv_hom{pixel[0], pixel[1], 1};
  Eigen::Vector3f Pc = Kinv * Puv_hom;
  Eigen::Vector3f ls = cam_T_enu * (Pc / Pc.norm());
  float d = height / (lr.transpose() * ls);
  Eigen::Vector3f Pt = ls * d;
  return Pt;
}

void draw_flow() {
#ifdef DRAW
  //######################    DRAWING
  cv::Mat drawing_frame = frame.clone();
  for (int y = 0; y < drawing_frame.rows; y += every_nth) {
    for (int x = 0; x < drawing_frame.cols; x += every_nth) {
      // Get the flow from `flow`, which is a 2-channel matrix
      const cv::Point2f &fxy = flow.at<cv::Point2f>(y, x);
      // Draw lines on `drawing_frame` to represent flow
      cv::line(drawing_frame, cv::Point(x, y), cv::Point(cvRound(x + fxy.x), cvRound(y + fxy.y)),
               cv::Scalar(0, 255, 0));
      cv::circle(drawing_frame, cv::Point(x, y), 2, cv::Scalar(0, 255, 0), -1);
    }
  }
  auto dominant_flow_vec = std::accumulate(flow_vecs.begin(), flow_vecs.end(), cv::Point2f(0, 0));
  dominant_flow_vec.y /= (float) flow_vecs.size();
  dominant_flow_vec.x /= (float) flow_vecs.size();
  // show the dominant flow vector on the image
  cv::line(drawing_frame, cv::Point(drawing_frame.cols / 2, drawing_frame.rows / 2),
           cv::Point(cvRound(drawing_frame.cols / 2 + dominant_flow_vec.x),
                     cvRound(drawing_frame.rows / 2 + dominant_flow_vec.y)),
           cv::Scalar(0, 0, 255), 5);
  // Display the image with vectors
  cv::imshow("Optical Flow Vectors", drawing_frame);
  cv::waitKey(1);
  //######################    DRAWING
#endif
}