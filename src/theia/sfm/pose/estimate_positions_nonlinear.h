// Copyright (C) 2015 The Regents of the University of California (Regents).
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//
//     * Neither the name of The Regents or University of California nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Please contact the author of this library if you have any questions.
// Author: Chris Sweeney (cmsweeney@cs.ucsb.edu)

#ifndef THEIA_SFM_POSE_ESTIMATE_POSITIONS_NONLINEAR_H_
#define THEIA_SFM_POSE_ESTIMATE_POSITIONS_NONLINEAR_H_

#include <ceres/ceres.h>
#include <Eigen/Core>
#include <memory>
#include <unordered_map>
#include <vector>

#include "theia/util/util.h"
#include "theia/sfm/reconstruction.h"
#include "theia/sfm/types.h"
#include "theia/sfm/view_triplet.h"

namespace theia {

struct NonlinearPositionEstimatorOptions {
  // Options for Ceres nonlinear solver.
  int num_threads = 1;
  bool verbose = false;
  int max_num_iterations = 400;
  double robust_loss_width = 1.0;

  // Minimum number of 3D points to camera correspondences for each
  // camera. These points can help constrain the problem and add robustness to
  // collinear configurations, but are not necessary to compute the position.
  int min_num_points_per_view = 0;

  // The total weight of all point to camera correspondences compared to camera
  // to camera correspondences.
  double point_to_camera_weight = 0.5;

  // Maximum number of reweighted iterations possible. Set to zero if no
  // reweighted iterations should be performed.
  int max_num_reweighted_iterations = 100;
  // A tolerance parameter to determine convergence of the iteratively
  // reweighted least squares.
  double reweighted_convergence_tolerance = 5e-3;
};

// Estimates the camera position of views given pairwise relative poses and the
// absolute orientations of cameras. Positions are estimated using a nonlinear
// solver with a robust cost function. If max_num_reweighted_iterations = 0 then
// a Huber loss function will be used to solve the problem.
//
// If max_num_reweighted_iterations > 0 then the class will perform an
// iteratively reweighted least squares solution. The first iteration will use a
// robust Huber loss function and consecutive iterations will use a standard L2
// loss with residuals weighted according to their current cost. This allows for
// a solution that is robust to outliers. The IRLS will stop upon convergence
// (based on total cost) or if the max number of iterations is reached.
class NonlinearPositionEstimator {
 public:
  NonlinearPositionEstimator(
      const NonlinearPositionEstimatorOptions& options,
      const Reconstruction& reconstruction,
      const std::unordered_map<ViewIdPair, TwoViewInfo>& view_pairs);

  // Returns true if the optimization was a success, false if there was a
  // failure.
  bool EstimatePositions(
      const std::unordered_map<ViewId, Eigen::Vector3d>& orientation,
      std::unordered_map<ViewId, Eigen::Vector3d>* positions);

 private:
  // Initialize all cameras to be random.
  void InitializeRandomPositions(
      const std::unordered_map<ViewId, Eigen::Vector3d>& orientations,
      std::unordered_map<ViewId, Eigen::Vector3d>* positions);

  // Creates camera to camera constraints from relative translations.
  void AddCameraToCameraConstraints(
      const std::unordered_map<ViewId, Eigen::Vector3d>& orientations,
      std::unordered_map<ViewId, Eigen::Vector3d>* positions);

  // Creates point to camera constraints.
  void AddPointToCameraConstraints(
      const std::unordered_map<ViewId, Eigen::Vector3d>& orientations,
      std::unordered_map<ViewId, Eigen::Vector3d>* positions);

  // Determines which tracks should be used for point to camera constraints. A
  // greedy approach is used so that the fewest number of tracks are chosen such
  // that all cameras have at least k point to camera constraints.
  int FindTracksForProblem(
      const std::unordered_map<ViewId, Eigen::Vector3d>& global_poses,
      std::unordered_set<TrackId>* tracks_to_add);

  // Sort the tracks by the number of views that observe them.
  std::vector<TrackId> GetTracksSortedByNumViews(
      const Reconstruction& reconstruction,
      const View& view,
      const std::unordered_set<TrackId>& existing_tracks);

  // Adds all point to camera constraints for a given track.
  void AddTrackToProblem(
      const TrackId track_id,
      const std::unordered_map<ViewId, Eigen::Vector3d>& orientations,
      const double point_to_camera_weight,
      std::unordered_map<ViewId, Eigen::Vector3d>* positions);

  // Reweight the loss functions based on the current residuals of the problem.
  void ReweightLossFunctions();

  const NonlinearPositionEstimatorOptions options_;
  const Reconstruction& reconstruction_;
  const std::unordered_map<ViewIdPair, TwoViewInfo>& view_pairs_;

  std::unordered_map<TrackId, Eigen::Vector3d> triangulated_points_;
  std::unique_ptr<ceres::Problem> problem_;
  ceres::Solver::Options solver_options_;

  // A container for the slack variables used to increase robustness in the
  // optimization.
  std::vector<double*> slack_variables_;

  // We keep track of the loss functions so that they may be used in the
  // iteratively reweighted least squares approach. The loss functions are
  // reweighted using a ScaledLoss.
  std::vector<ceres::LossFunctionWrapper*> loss_functions_;

  friend class EstimatePositionsNonlinearTest;

  DISALLOW_COPY_AND_ASSIGN(NonlinearPositionEstimator);
};

}  // namespace theia

#endif  // THEIA_SFM_POSE_ESTIMATE_POSITIONS_NONLINEAR_H_
