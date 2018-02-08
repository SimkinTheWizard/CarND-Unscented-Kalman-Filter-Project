#include <iostream>
#include "tools.h"

using Eigen::VectorXd;
using Eigen::MatrixXd;
using std::vector;

Tools::Tools() {}

Tools::~Tools() {}

VectorXd Tools::CalculateRMSE(const vector<VectorXd> &estimations,
                              const vector<VectorXd> &ground_truth) {
  /**
  TODO:
    * Calculate the RMSE here.
  */
	VectorXd error(4);
	error << 0,0,0,0;
	if (estimations.size()== 0)
		return error;
	if (estimations.size() != ground_truth.size())
		return error;
	for (int i = 0; i < estimations.size(); i++)
	{
		VectorXd intermediate = estimations[i] - ground_truth[i];
		intermediate = intermediate.array() * intermediate.array();
		error += intermediate;
	}
	error = error /estimations.size();
	return error.array().sqrt();
}
