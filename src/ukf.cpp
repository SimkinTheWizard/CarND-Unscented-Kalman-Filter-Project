#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

#define PI 3.14

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
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
  std_a_ = 3;

  // Process noise standard deviation yaw acceleration in rad/s^2
  // The car completes 2*PI turn in approx. 10 seconds so PI/10 is a good value
  std_yawdd_ = PI/10;
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
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
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
	n_x_ = 5;
	n_aug_ = n_x_ + 2;
	
	x_ = Eigen::VectorXd(n_x_);
	Xsig_pred_ = Eigen::MatrixXd(n_x_, 2*n_aug_+1);
	Xsig_aug_ = Eigen::MatrixXd(n_aug_,2*n_aug_+1);
	
	lambda_ = 3 - n_aug_;
	
	weights_ = VectorXd(2*n_aug_+1);
	weights_(0) = lambda_ / (lambda_ + n_aug_);
	for (int i = 1; i < 2*n_aug_ + 1; i++)
	{
		weights_(i) = 1 /(2* (lambda_ + n_aug_) );
	}
	
	is_initialized_ = false;
	
	H_lidar_ = Eigen::MatrixXd(2,n_x_);
	H_lidar_ << 1, 0, 0, 0, 0,
	            0, 1, 0, 0, 0;
	
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
	if (!is_initialized_)
	{
		if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
		{
			float rho = meas_package.raw_measurements_[0];
			float theta = meas_package.raw_measurements_[1];
			float rho_dot = meas_package.raw_measurements_[2];
			
			
			float px = rho * cos(theta);
			float py = rho * sin(theta);
			float vx = rho_dot * cos(theta);
			float vy = rho_dot * sin(theta);
			
			x_ << px,
			      py,
			      sqrt(vx*vx + vy*vy),
			      vx < 0.001? 0: atan(vy/vx),
			      0;
		}
		else if (meas_package.sensor_type_ == MeasurementPackage::LASER)
		{
			x_ << meas_package.raw_measurements_[0],
			      meas_package.raw_measurements_[1],
			      0,
			      0,
			      0;
		}
		
		P_ << 1, 0, 0, 0, 0,
		      0, 1, 0, 0, 0,
			  0, 0, 1, 0, 0,
		      0, 0, 0, 1, 0,
		      0, 0, 0, 0, 1;
		
		time_us_ = meas_package.timestamp_;
		is_initialized_ = true;
	}
	float dt  = (meas_package.timestamp_ - time_us_) / 1000000.0;
	time_us_ = meas_package.timestamp_;
	
	//Prediction(dt);
	
	if (meas_package.sensor_type_ == MeasurementPackage::RADAR)
	{
		if (use_radar_)
		{
			Prediction(dt);
			UpdateRadar(meas_package);
		}
	}
	else if (meas_package.sensor_type_ == MeasurementPackage::LASER)
	{
		if(use_laser_)
		{
			Prediction(dt);
			UpdateLidar(meas_package);
		}
	}
	
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::GenerateSigmaPoints()
{
	// Generate augmented sigma points
	MatrixXd z1 = MatrixXd::Constant(n_x_,n_aug_-n_x_,0);
	MatrixXd z2 = MatrixXd::Constant(n_aug_-n_x_,n_x_,0);
	
	MatrixXd Q(2,2);
	Q << std_a_*std_a_, 0,
	0, std_yawdd_*std_yawdd_;
	
	MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
	P_aug << P_, z1,z2,Q;
	
	MatrixXd B = MatrixXd(n_aug_, n_aug_);
	B = P_aug.llt().matrixL();
	
	Eigen::VectorXd x_aug = Eigen::VectorXd(n_aug_);
	for (int i = 0; i<n_aug_; i++)
	{
		if (i < n_x_)
			x_aug(i) = x_(i);
		else
			x_aug(i) = 0;
	}
	MatrixXd X7= MatrixXd(7,7);
	for (int i =0; i<n_aug_; i++)
		X7.col(i) = x_aug;
	
	Xsig_aug_ << x_aug, X7+(sqrt(lambda_+n_aug_)*B),  (X7-sqrt(lambda_+n_aug_)*B);
	
}

void UKF::PredictSigmaPoints(double delta_t)
{
	for (int i = 0; i< 2*n_aug_ + 1; i++)
	{
		double mu_psy  = Xsig_aug_(6,i);
		double mu_a    = Xsig_aug_(5,i);
		double psy_dot = Xsig_aug_(4,i);
		double psy     = Xsig_aug_(3,i);
		double v       = Xsig_aug_(2,i);
		double y       = Xsig_aug_(1,i);
		double x       = Xsig_aug_(0,i);
		
		VectorXd additional1(n_x_);
		VectorXd additional2(n_x_);
		VectorXd x_i(n_x_);
		
		x_i << x, y, v, psy, psy_dot;
		
		additional2 << 0.5*delta_t*delta_t*cos(psy)*mu_a,
		               0.5*delta_t*delta_t*sin(psy)*mu_a,
		               delta_t*mu_a,
		               0.5*delta_t*delta_t*mu_psy,
		               delta_t*mu_psy;
		
		if (psy_dot  > 0.01)
		{
			additional1 <<  v/psy_dot * ( sin (psy + psy_dot*delta_t) - sin(psy)),
			v/psy_dot*( -cos(psy + psy_dot * delta_t)+ cos(psy) ),
			0,
			psy_dot *delta_t,
			0;
		}
		else
		{
			additional1 <<  v*cos(psy)*delta_t,
			v*sin(psy)*delta_t,
			0,
			psy_dot*delta_t,
			0;
		}

		Xsig_pred_.col(i) = x_i + additional1 + additional2;
		
	}
	
	
}
void UKF::PredictMeanAndCovariance()
{

	x_.fill(0.0);
	for (int i = 0; i < 2*n_aug_ + 1; i++)
	{
		x_ = x_ + Xsig_pred_.col(i) * weights_(i);
	}
	P_.fill(0.0);
	for (int i = 0; i < 2*n_aug_ + 1; i++)
	{
		P_ = P_ +  weights_(i) * (Xsig_pred_.col(i)-x_) *(Xsig_pred_.col(i)-x_).transpose() ;
	}
	
}
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
	GenerateSigmaPoints();
	PredictSigmaPoints(delta_t);
	PredictMeanAndCovariance();
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
	//PredictLidarMeasurements();
	//UdpdateState(meas_package);
	UpdateLinear(meas_package.raw_measurements_);
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
	PredictRadarMeasurements();
	UdpdateState(meas_package);
}

void UKF::PredictLidarMeasurements() {
	n_z_ = 2;
	z_pred = VectorXd(n_z_);
	Zsig = MatrixXd(n_z_, 2 * n_aug_ + 1);
	S = MatrixXd(n_z_,n_z_);
	for (int i = 0; i<2*n_aug_ + 1; i++)
	{
		double px      = Xsig_pred_(0,i);
		double py      = Xsig_pred_(1,i);
		double v       = Xsig_pred_(2,i);
		double psy     = Xsig_pred_(3,i);
		double psy_dot = Xsig_pred_(4,i);
		
		Zsig.col(i) << px, py;
		// unused parameters
		(void) (v);
		(void) (psy);
		(void) (psy_dot);
	}
	
	z_pred.fill(0.0);
	for (int i = 0; i<2*n_aug_ + 1; i++)
	{
		z_pred += Zsig.col(i) * weights_(i);
	}
	
	MatrixXd R = MatrixXd(n_z_,n_z_);
	R << std_laspx_*std_laspx_, 0 ,
	     0, std_laspy_*std_laspy_;
	
	S.fill(0.0);
	for (int i = 0; i<2*n_aug_ + 1; i++)
	{
		S += (Zsig.col(i)-z_pred)*(Zsig.col(i)-z_pred).transpose()* weights_(i);
	}
	S += R;

}



void UKF::PredictRadarMeasurements() {
	n_z_ = 3;
	z_pred = VectorXd(n_z_);
	Zsig = MatrixXd(n_z_, 2 * n_aug_ + 1);
	S = MatrixXd(n_z_,n_z_);
	for (int i = 0; i<2*n_aug_ + 1; i++)
	{
		double px      = Xsig_pred_(0,i);
		double py      = Xsig_pred_(1,i);
		double v       = Xsig_pred_(2,i);
		double psy     = Xsig_pred_(3,i);
		double psy_dot = Xsig_pred_(4,i);
		
		double rho = sqrt (px*px + py*py);
		double theta = px < 0.01 ? 0 : atan(py/px);
		double rho_dot = rho > 0.01 ? (px*cos(psy)*v + py*sin(psy)*v)/sqrt (px*px + py*py) : 0;
		
		//while (theta>2*PI)
		//	theta -= 2*PI;
		//while (theta<-2*PI)
		//	theta += 2*PI;

        Zsig.col(i) << rho,
		               theta,
		               rho_dot;
 

		// unused parameter
		(void) (psy_dot);
	}
	
	z_pred.fill(0.0);
	for (int i = 0; i<2*n_aug_ + 1; i++)
	{
		z_pred += Zsig.col(i) * weights_(i);
	}
	
	MatrixXd R = MatrixXd(n_z_,n_z_);
	R << std_radr_*std_radr_, 0 , 0,
	     0, std_radphi_*std_radphi_, 0,
	     0, 0, std_radrd_*std_radrd_;
	
	S.fill(0.0);
	for (int i = 0; i<2*n_aug_ + 1; i++)
	{
		S += (Zsig.col(i)-z_pred)*(Zsig.col(i)-z_pred).transpose()* weights_(i);
	}
	S += R;
	
	


}

void UKF::UdpdateState(MeasurementPackage meas_package) {
	
	MatrixXd Tc = MatrixXd(n_x_, n_z_);
	Eigen::MatrixXd z ;
	z = meas_package.raw_measurements_;
	Tc.fill(0.0);
	for (int i = 0; i< 2*n_aug_+1 ; i++)
	{
		Tc += weights_(i) * (Xsig_pred_.col(i) - x_) * (Zsig.col(i) - z_pred).transpose();
	}
	
	MatrixXd K =  Tc* S.inverse();
	
	x_ = x_ + K * (z-z_pred);
	
	P_ = P_ - K * S * K.transpose();
	
	
}



void UKF::UpdateLinear(const VectorXd &z) {
	/**
	 TODO:
	 * update the state by using Kalman Filter equations
	 */
	n_z_ = 2;
	MatrixXd R = MatrixXd(n_z_,n_z_);
	R << std_laspx_*std_laspx_, 0 ,
	0, std_laspy_*std_laspy_;
	
	VectorXd z_pred = H_lidar_ * x_;
	VectorXd y = z - z_pred;
	MatrixXd Ht = H_lidar_.transpose();
	MatrixXd S = H_lidar_ * P_ * Ht + R;
	MatrixXd Si = S.inverse();
	MatrixXd PHt = P_ * Ht;
	MatrixXd K = PHt * Si;
	
	//new estimate
	x_ = x_ + (K * y);
	long x_size = x_.size();
	MatrixXd I = MatrixXd::Identity(x_size, x_size);
	P_ = (I - K * H_lidar_) * P_;
}








