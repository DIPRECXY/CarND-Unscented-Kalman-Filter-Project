#include "ukf.h"
#include "tools.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 5;//30

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 3;//30

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
	
  // the current NIS for radar
  NIS_radar_ = 0.0;

  // the current NIS for laser
  NIS_laser_ = 0.0;

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
  n_x_ = 5;
  n_aug_ = n_x_ + 2;
  lambda_ = 3 - n_aug_;
  P_ << 1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0,
        0, 0, 0, 0, 1;
  
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ +1);
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  
  // Initialization
  if (!is_initialized_) {
    // first measurement
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      // Convert radar from polar to cartesian coordinates 
      // Initialize state
      float ro = meas_package.raw_measurements_(0);
      float phi = meas_package.raw_measurements_(1);
      float ro_dot = meas_package.raw_measurements_(2);
      x_(0) = ro * cos(phi);
      x_(1) = ro * sin(phi);
      x_(2) = ro_dot * cos(phi); 
      x_(3) = ro_dot * sin(phi);
      x_(4) = 0;
      if (fabs(ro) > 0.01) {
        is_initialized_ = true;
      }
    }else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      // Initialize state
      x_(0) = meas_package.raw_measurements_(0);
      x_(1) = meas_package.raw_measurements_(1);
      x_(2) = 0;
      x_(3) = 0;
      x_(4) = 0;
      if (sqrt(pow(x_(0),2) + pow(x_(1),2)) > 0.001) {
        is_initialized_ = true;
      }
    }
    // initializing
    time_us_ = meas_package.timestamp_;
    return;
  }
  // compute time
  double dt = (meas_package.timestamp_ - time_us_) / 1000000.0;
  time_us_ = meas_package.timestamp_;
  
  // Prediction
  const double diff_t = 0.01;
  while (dt > diff_t) {
    Prediction(diff_t);
    dt -= diff_t;
  }
  Prediction(dt);
  
  // Update
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
  // Radar update
    if (fabs(meas_package.raw_measurements_(0)) > 0.001) {
      if (use_radar_) {
        UpdateRadar(meas_package);
      }
    }
  } else {
  // Laser Update
    double l_px = meas_package.raw_measurements_(0);
    double l_py = meas_package.raw_measurements_(1);
    if (sqrt(pow(l_px,2) + pow(l_py,2)) > 0.001) {
      if (use_laser_) {
      UpdateLidar(meas_package);
      }
    }
  }
    cout << "x_ = " << x_ << endl;
    cout << "P_ = " << P_ << endl;
  
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  // Generate sigma points
  VectorXd x_aug = VectorXd(n_aug_);
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
  
  // mean sate
  x_aug.head(n_x_) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;
  
  // covariance matrix
  P_aug.fill(0.0);
  P_aug.topLeftCorner(n_x_, n_x_) = P_;
  P_aug(5,5) = std_a_ * std_a_;
  P_aug(6,6) = std_yawdd_ * std_yawdd_;
  
  // square root matrix
  MatrixXd L = P_aug.llt().matrixL();
  
  // aug sigma points
  Xsig_aug.col(0) = x_aug;
  for (int i=0; i< n_aug_; i++) 
  {
    Xsig_aug.col(i+1)          = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_)   = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
  }
  MatrixXd Xsig_pred = MatrixXd(n_x_, 2 * n_aug_ + 1);
  
  // predict sigma points
  for (int i = 0; i< 2*n_aug_+1; i++)
  {
    //extract values for better readability
    double p_x = Xsig_aug(0,i);
    double p_y = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawd = Xsig_aug(4,i);
    double nu_a = Xsig_aug(5,i);
    double nu_yawdd = Xsig_aug(6,i);

    //predicted state values
    double px_p, py_p;

    //avoid division by zero
    if (fabs(yawd) > 0.001) {
        px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
        py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
    }
    else {
        px_p = p_x + v*delta_t*cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;

    //add noise
    px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
    py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    v_p = v_p + nu_a*delta_t;

    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;

    //write predicted sigma point into right column
    Xsig_pred(0,i) = px_p;
    Xsig_pred(1,i) = py_p;
    Xsig_pred(2,i) = v_p;
    Xsig_pred(3,i) = yaw_p;
    Xsig_pred(4,i) = yawd_p;
  }
  
  //create vector for weights
  VectorXd weights = VectorXd(2*n_aug_+1);
  
  //create vector for predicted state
  //VectorXd x = VectorXd(n_x_);

  //create covariance matrix for prediction
  //MatrixXd P = MatrixXd(n_x_, n_x_);
  
  // set weights
  double weight_0 = lambda_/(lambda_+n_aug_);
  weights(0) = weight_0;
  for (int i=1; i<2*n_aug_+1; i++) {  //2n+1 weights
    double weight = 0.5/(n_aug_+lambda_);
    weights(i) = weight;
  }

  //predicted state mean
  x_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
    x_ = x_+ weights(i) * Xsig_pred.col(i);
  }

  //predicted state covariance matrix
  P_.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points

    // state difference
    VectorXd x_diff = Xsig_pred.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    P_ = P_ + weights(i) * x_diff * x_diff.transpose() ;
	  Xsig_pred_ = Xsig_pred;
  }
  
  
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */

// Radar measurement - function
VectorXd MeasurementRadarFuntion(VectorXd x){
	//extract values for better readability
  double p_x = x(0);
  double p_y = x(1);
  double v = x(2);
  double yaw = x(3);
	
  double v1 = cos(yaw)*v;
  double v2 = sin(yaw)*v;
  
  VectorXd Zsig = VectorXd(3);
  
  if (sqrt(p_x*p_x + p_y*p_y) >0.001) {
    // measurement model
    Zsig(0) = sqrt(p_x*p_x + p_y*p_y);                        //r
    Zsig(1) = atan2(p_y,p_x);                                 //phi
    Zsig(2) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
	return Zsig;
  }
  else {
    Zsig(0) = 0.001;
    Zsig(1) = 0;
    Zsig(2) =  (p_x*v1 + p_y*v2 ) / Zsig(0);
    return Zsig;
  }
	
}

// LIDAR measurement - function
VectorXd MeasurementLidarFunction(VectorXd x){
    double  p_x = x(0);
    double  p_y = x(1);
    VectorXd Zsig = VectorXd(2);
    Zsig(0) = p_x;
    Zsig(1) = p_y;
    return Zsig;
}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  //set measurement dimension, radar can measure r, phi, and r_dot
  int n_z = 2;
  
  //set vector for weights
  VectorXd weights = VectorXd(2*n_aug_+1);
  double weight_0 = lambda_/(lambda_+n_aug_);
  weights(0) = weight_0;
  for (int i=1; i<2*n_aug_+1; i++) {
    double weight = 0.5 / (n_aug_ + lambda_);
    weights(i) = weight;
  }
  
  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  for (int i=0;i<2*n_aug_+1;i++){
      Zsig.col(i) = MeasurementLidarFunction(Xsig_pred_.col(i));
  }
  
  
  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      z_pred = z_pred + weights(i) * Zsig.col(i);
  }

  //measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights(i) * z_diff * z_diff.transpose();
  }

  
  //add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z,n_z);
  R <<    std_laspx_*std_laspx_,0,
          0,std_laspy_*std_laspy_;
  S = S + R;

  MatrixXd Tc = MatrixXd(n_x_, n_z);

   //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z_ = VectorXd(n_z);
  z_ = meas_package.raw_measurements_;
  VectorXd z_diff = z_ - z_pred;
  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();
  NIS_laser_ = z_diff.transpose()*S*z_diff;
  //cout << "NIS_laser_:  " << NIS_laser_ << endl;
  
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  //set measurement dimension, radar can measure r, phi, and r_dot
  int n_z = 3;
  //set vector for weights
  VectorXd weights = VectorXd(2*n_aug_+1);
  
  //create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);
  
  //measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  
  for (int i=0;i<2*n_aug_+1;i++){
      Zsig.col(i) = MeasurementRadarFuntion(Xsig_pred_.col(i));
  }
  
  double weight_0 = lambda_/(lambda_+n_aug_);
  weights(0) = weight_0;
  for (int i=1; i<2*n_aug_+1; i++) {
    double weight = 0.5/(n_aug_+lambda_);
    weights(i) = weight;
  }
  
  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      z_pred = z_pred + weights(i) * Zsig.col(i);
  }

  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights(i) * z_diff * z_diff.transpose();
  }
  
  //add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z,n_z);
  R <<    std_radr_*std_radr_, 0, 0,
          0, std_radphi_*std_radphi_, 0,
          0, 0,std_radrd_*std_radrd_;
  S = S + R;

  MatrixXd Tc = MatrixXd(n_x_, n_z);
  
   //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    //residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights(i) * x_diff * z_diff.transpose();
  }


  //Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  //residual
  VectorXd z_ = VectorXd(n_z);
  z_ = meas_package.raw_measurements_;
  VectorXd z_diff = z_ - z_pred;


  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();

    NIS_radar_ = z_diff.transpose()*S*z_diff;
  //cout << "NIS_radar_:  " << NIS_radar_ << endl;
  
}
