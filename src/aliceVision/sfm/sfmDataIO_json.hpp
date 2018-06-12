// This file is part of the AliceVision project.
// Copyright (c) 2016 AliceVision contributors.
// Copyright (c) 2012 openMVG contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <aliceVision/sfm/sfmDataIO.hpp>

#include <boost/property_tree/ptree.hpp>

#include <string>

namespace aliceVision {
namespace sfm {

namespace bpt = boost::property_tree;

/**
 * @brief Save an Eigen Matrix (or Vector) in a boost property tree.
 * @param[in] name The node name ( "" = no name )
 * @param[in] matrix The input matrix
 * @param[out] parentTree The parent tree
 */
template<typename Derived>
inline void saveMatrix(const std::string& name, const Eigen::MatrixBase<Derived>& matrix, bpt::ptree& parentTree)
{
  bpt::ptree matrixTree;

  const int size = matrix.size();
  for(int i = 0; i < size; ++i)
  {
    bpt::ptree cellTree;
    cellTree.put("", matrix(i));
    matrixTree.push_back(std::make_pair("", cellTree));
  }

  parentTree.add_child(name, matrixTree);
}

/**
 * @brief Load an Eigen Matrix (or Vector) from a boost property tree.
 * @param[in] name The Matrix name ( "" = no name )
 * @param[out] matrix The output Matrix
 * @param[in,out] matrixTree The input tree
 */
template<typename Derived>
inline void loadMatrix(const std::string& name, Eigen::MatrixBase<Derived>& matrix, bpt::ptree& matrixTree)
{
  const int size = matrix.size();
  int i = 0;

  for(bpt::ptree::value_type &cellNode : matrixTree.get_child(name))
  {
    if(i > size)
      throw std::out_of_range("Invalid matrix / vector type for : " + name);

    matrix(i) = cellNode.second.get_value<typename Derived::Scalar>();
    ++i;
  }
}

/**
 * @brief Save a Pose3 in a boost property tree.
 * @param[in] name The node name ( "" = no name )
 * @param[in] pose The input pose3
 * @param[out] parentTree The parent tree
 */
inline void savePose3(const std::string& name, const geometry::Pose3& pose, bpt::ptree& parentTree)
{
  bpt::ptree pose3Tree;

  saveMatrix("rotation", pose.rotation(), pose3Tree);
  saveMatrix("center", pose.center(), pose3Tree);

  parentTree.add_child(name, pose3Tree);
}

/**
 * @brief Load a Pose3 from a boost property tree.
 * @param[in] name The Pose3 name ( "" = no name )
 * @param[out] pose The output Pose3
 * @param[in,out] pose3Tree The input tree
 */
inline void loadPose3(const std::string& name, geometry::Pose3& pose, bpt::ptree& pose3Tree)
{
  Mat3 rotation;
  Vec3 center;

  loadMatrix(name + ".rotation", rotation, pose3Tree);
  loadMatrix(name + ".center",   center, pose3Tree);

  pose = geometry::Pose3(rotation, center);
}

/**
 * @brief Save a Pose3 in a boost property tree.
 * @param[in] name The node name ( "" = no name )
 * @param[in] pose The input pose3
 * @param[out] parentTree The parent tree
 */
inline void saveCameraPose(const std::string& name, const CameraPose& cameraPose, bpt::ptree& parentTree)
{
  bpt::ptree cameraPoseTree;

  savePose3("transform", cameraPose.getTransform(), cameraPoseTree);
  cameraPoseTree.put("locked", cameraPose.isLocked());

  parentTree.add_child(name, cameraPoseTree);
}

/**
 * @brief Load a Pose3 from a boost property tree.
 * @param[in] name The Pose3 name ( "" = no name )
 * @param[out] pose The output Pose3
 * @param[in,out] pose3Tree The input tree
 */
inline void loadCameraPose(const std::string& name, CameraPose& cameraPose, bpt::ptree& cameraPoseTree)
{
  geometry::Pose3 pose;

  loadPose3(name + ".transform", pose, cameraPoseTree);
  cameraPose.setTransform(pose);

  if(cameraPoseTree.get<bool>("locked", false))
    cameraPose.lock();
  else
    cameraPose.unlock();
}

/**
 * @brief Save a View in a boost property tree.
 * @param[in] name The node name ( "" = no name )
 * @param[in] view The input View
 * @param[out] parentTree The parent tree
 */
void saveView(const std::string& name, const View& view, bpt::ptree& parentTree);

/**
 * @brief Load a View from a boost property tree.
 * @param[out] view The output View
 * @param[in,out] viewTree The input tree
 */
void loadView(View& view, bpt::ptree& viewTree);

/**
 * @brief Save an Intrinsic in a boost property tree.
 * @param[in] name The node name ( "" = no name )
 * @param[in] intrinsicId The intrinsic Id
 * @param[in] intrinsic The intrinsic
 * @param[out] parentTree The parent tree
 */
void saveIntrinsic(const std::string& name, IndexT intrinsicId, const std::shared_ptr<camera::IntrinsicBase>& intrinsic, bpt::ptree& parentTree);

/**
 * @brief Load an Intrinsic from a boost property tree.
 * @param[out] intrinsicId The output Intrinsic Id
 * @param[out] intrinsic The output Intrinsic
 * @param intrinsicTree The input tree
 */
void loadIntrinsic(IndexT& intrinsicId, std::shared_ptr<camera::IntrinsicBase>& intrinsic, bpt::ptree& intrinsicTree);

/**
 * @brief Save a Rig in a boost property tree.
 * @param[in] name The node name ( "" = no name )
 * @param[in] rigId The rig Id
 * @param[in] rig The rig
 * @param[out] parentTree The parent tree
 */
void saveRig(const std::string& name, IndexT rigId, const Rig& rig, bpt::ptree& parentTree);

/**
 * @brief Load a Rig from a boost property tree.
 * @param[out] rigId The output Rig Id
 * @param[out] rig The output Rig
 * @param[in,out] rigTree The input tree
 */
void loadRig(IndexT& rigId, Rig& rig, bpt::ptree& rigTree);

/**
 * @brief Save a Landmark in a boost property tree.
 * @param[in] name The node name ( "" = no name )
 * @param[in] landmarkId The landmark Id
 * @param[in] landmark The landmark
 * @param[out] parentTree The parent tree
 */
void saveLandmark(const std::string& name, IndexT landmarkId, const Landmark& landmark, bpt::ptree& parentTree);

/**
 * @brief Load a Landmark from a boost property tree.
 * @param[out] landmarkId The output Landmark Id
 * @param[out] landmark The output Landmmark
 * @param[in,out] landmarkTree The input tree
 */
void loadLandmark(IndexT& landmarkId, Landmark& landmark, bpt::ptree& landmarkTree);

/**
 * @brief Save an SfMData in a JSON file with a boost property tree.
 * @param[in] sfmData The input SfMData
 * @param[in] filename The filename
 * @param[in] partFlag The ESfMData save flag
 * @return true if completed
 */
bool saveJSON(const SfMData& sfmData, const std::string& filename, ESfMData partFlag);

/**
 * @brief Load a JSON SfMData file.
 * @param[out] sfmData The output SfMData
 * @param[in] filename The filename
 * @param[in] partFlag The ESfMData load flag
 * @param[in] incompleteViews If true, try to load incomplete views
 * @return true if completed
 */
bool loadJSON(SfMData& sfmData, const std::string& filename, ESfMData partFlag, bool incompleteViews = false);

} // namespace sfm
} // namespace aliceVision
