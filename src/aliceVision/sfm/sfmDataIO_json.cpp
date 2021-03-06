// This file is part of the AliceVision project.
// Copyright (c) 2017 AliceVision contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "sfmDataIO_json.hpp"
#include <aliceVision/camera/camera.hpp>
#include <aliceVision/sfm/viewIO.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <memory>
#include <cassert>

namespace aliceVision {
namespace sfm {

void saveView(const std::string& name, const View& view, bpt::ptree& parentTree)
{
  bpt::ptree viewTree;

  if(view.getViewId() != UndefinedIndexT)
    viewTree.put("viewId", view.getViewId());

  if(view.getPoseId() != UndefinedIndexT)
    viewTree.put("poseId", view.getPoseId());

  if(view.isPartOfRig())
  {
    viewTree.put("rigId", view.getRigId());
    viewTree.put("subPoseId", view.getSubPoseId());
  }

  if(view.getIntrinsicId() != UndefinedIndexT)
    viewTree.put("intrinsicId", view.getIntrinsicId());

  if(view.getResectionId() != UndefinedIndexT)
    viewTree.put("resectionId", view.getResectionId());

  viewTree.put("path", view.getImagePath());
  viewTree.put("width", view.getWidth());
  viewTree.put("height", view.getHeight());

  // metadata
  {
    bpt::ptree metadataTree;

    for(const auto& metadataPair : view.getMetadata())
      metadataTree.put(metadataPair.first, metadataPair.second);

    viewTree.add_child("metadata", metadataTree);
  }

  parentTree.push_back(std::make_pair(name, viewTree));
}

void loadView(View& view, bpt::ptree& viewTree)
{
  view.setViewId(viewTree.get<IndexT>("viewId", UndefinedIndexT));
  view.setPoseId(viewTree.get<IndexT>("poseId", UndefinedIndexT));

  if(viewTree.count("rigId"))
  {
    view.setRigAndSubPoseId(
      viewTree.get<IndexT>("rigId"),
      viewTree.get<IndexT>("subPoseId"));
  }

  view.setIntrinsicId(viewTree.get<IndexT>("intrinsicId", UndefinedIndexT));
  view.setResectionId(viewTree.get<IndexT>("resectionId", UndefinedIndexT));

  view.setImagePath(viewTree.get<std::string>("path"));
  view.setWidth(viewTree.get<std::size_t>("width", 0));
  view.setHeight(viewTree.get<std::size_t>("height", 0));

  // metadata
  if(viewTree.count("metadata"))
    for(bpt::ptree::value_type &metaDataNode : viewTree.get_child("metadata"))
      view.addMetadata(metaDataNode.first, metaDataNode.second.data());

}

void saveIntrinsic(const std::string& name, IndexT intrinsicId, const std::shared_ptr<camera::IntrinsicBase>& intrinsic, bpt::ptree& parentTree)
{
  bpt::ptree intrinsicTree;

  camera::EINTRINSIC intrinsicType = intrinsic->getType();

  intrinsicTree.put("intrinsicId", intrinsicId);
  intrinsicTree.put("width", intrinsic->w());
  intrinsicTree.put("height", intrinsic->h());
  intrinsicTree.put("type", camera::EINTRINSIC_enumToString(intrinsicType));
  intrinsicTree.put("serialNumber", intrinsic->serialNumber());
  intrinsicTree.put("pxInitialFocalLength", intrinsic->initialFocalLengthPix());

  if(camera::isPinhole(intrinsicType))
  {
    const camera::Pinhole& pinholeIntrinsic = dynamic_cast<camera::Pinhole&>(*intrinsic);

    intrinsicTree.put("pxFocalLength", pinholeIntrinsic.getFocalLengthPix());
    saveMatrix("principalPoint", pinholeIntrinsic.getPrincipalPoint(), intrinsicTree);

    bpt::ptree distParamsTree;

    for(double param : pinholeIntrinsic.getDistortionParams())
    {
      bpt::ptree paramTree;
      paramTree.put("", param);
      distParamsTree.push_back(std::make_pair("", paramTree));
    }

    intrinsicTree.add_child("distortionParams", distParamsTree);
  }

  parentTree.push_back(std::make_pair(name, intrinsicTree));
}

void loadIntrinsic(IndexT& intrinsicId, std::shared_ptr<camera::IntrinsicBase>& intrinsic, bpt::ptree& intrinsicTree)
{
  intrinsicId = intrinsicTree.get<IndexT>("intrinsicId");
  const unsigned int width = intrinsicTree.get<unsigned int>("width");
  const unsigned int height = intrinsicTree.get<unsigned int>("height");
  const camera::EINTRINSIC intrinsicType = camera::EINTRINSIC_stringToEnum(intrinsicTree.get<std::string>("type"));
  const double pxFocalLength = intrinsicTree.get<double>("pxFocalLength");

  // principal point
  Vec2 principalPoint;
  loadMatrix("principalPoint", principalPoint, intrinsicTree);

  // check if the camera is a Pinhole model
  if(!camera::isPinhole(intrinsicType))
    throw std::out_of_range("Only Pinhole camera model supported");

  // pinhole parameters
  std::shared_ptr<camera::Pinhole> pinholeIntrinsic = camera::createPinholeIntrinsic(intrinsicType, width, height, pxFocalLength, principalPoint(0), principalPoint(1));
  pinholeIntrinsic->setInitialFocalLengthPix(intrinsicTree.get<double>("pxInitialFocalLength"));
  pinholeIntrinsic->setSerialNumber(intrinsicTree.get<std::string>("serialNumber"));

  std::vector<double> distortionParams;

  for(bpt::ptree::value_type &paramNode : intrinsicTree.get_child("distortionParams"))
    distortionParams.emplace_back(paramNode.second.get_value<double>());

  pinholeIntrinsic->setDistortionParams(distortionParams);
  intrinsic = std::static_pointer_cast<camera::IntrinsicBase>(pinholeIntrinsic);
}

void saveRig(const std::string& name, IndexT rigId, const Rig& rig, bpt::ptree& parentTree)
{
  bpt::ptree rigTree;

  rigTree.put("rigId", rigId);

  bpt::ptree rigSubPosesTree;

  for(const auto& rigSubPose : rig.getSubPoses())
  {
    bpt::ptree rigSubPoseTree;

    rigSubPoseTree.put("status", ERigSubPoseStatus_enumToString(rigSubPose.status));
    savePose3("pose", rigSubPose.pose, rigSubPoseTree);

     rigSubPosesTree.push_back(std::make_pair("", rigSubPoseTree));
  }

  rigTree.add_child("subPoses", rigSubPosesTree);

  parentTree.push_back(std::make_pair(name, rigTree));
}


void loadRig(IndexT& rigId, Rig& rig, bpt::ptree& rigTree)
{
  rigId =  rigTree.get<IndexT>("rigId");
  rig = Rig(rigTree.get_child("subPoses").size());
  int subPoseId = 0;

  for(bpt::ptree::value_type &subPoseNode : rigTree.get_child("subPoses"))
  {
    bpt::ptree& subPoseTree = subPoseNode.second;

    RigSubPose subPose;

    subPose.status = ERigSubPoseStatus_stringToEnum(subPoseTree.get<std::string>("status"));
    loadPose3("pose", subPose.pose, subPoseTree);

    rig.setSubPose(subPoseId++, subPose);
  }
}

void saveLandmark(const std::string& name, IndexT landmarkId, const Landmark& landmark, bpt::ptree& parentTree)
{
  bpt::ptree landmarkTree;

  landmarkTree.put("landmarkId", landmarkId);
  landmarkTree.put("descType", feature::EImageDescriberType_enumToString(landmark.descType));

  saveMatrix("color", landmark.rgb, landmarkTree);
  saveMatrix("X", landmark.X, landmarkTree);

  // observations
  bpt::ptree observationsTree;
  for(const auto& obsPair : landmark.observations)
  {
    bpt::ptree obsTree;

    const Observation& observation = obsPair.second;

    obsTree.put("observationId", obsPair.first);
    obsTree.put("featureId", observation.id_feat);

    saveMatrix("x", observation.x, obsTree);

    observationsTree.push_back(std::make_pair("", obsTree));
  }

  landmarkTree.add_child("observations", observationsTree);

  parentTree.push_back(std::make_pair(name, landmarkTree));
}

void loadLandmark(IndexT& landmarkId, Landmark& landmark, bpt::ptree& landmarkTree)
{
  landmarkId = landmarkTree.get<IndexT>("landmarkId");
  landmark.descType = feature::EImageDescriberType_stringToEnum(landmarkTree.get<std::string>("descType"));

  loadMatrix("color", landmark.rgb, landmarkTree);
  loadMatrix("X", landmark.X, landmarkTree);

  // observations
  for(bpt::ptree::value_type &obsNode : landmarkTree.get_child("observations"))
  {
    bpt::ptree& obsTree = obsNode.second;

    Observation observation;

    observation.id_feat = obsTree.get<IndexT>("featureId");
    loadMatrix("x", observation.x, obsTree);

    landmark.observations.emplace(obsTree.get<IndexT>("observationId"), observation);
  }
}


bool saveJSON(const SfMData& sfmData, const std::string& filename, ESfMData partFlag)
{
  const Vec3 version = {1, 0, 0};

  // save flags
  const bool saveViews = (partFlag & VIEWS) == VIEWS;
  const bool saveIntrinsics = (partFlag & INTRINSICS) == INTRINSICS;
  const bool saveExtrinsics = (partFlag & EXTRINSICS) == EXTRINSICS;
  const bool saveStructure = (partFlag & STRUCTURE) == STRUCTURE;
  const bool saveControlPoints = (partFlag & CONTROL_POINTS) == CONTROL_POINTS;

  // main tree
  bpt::ptree fileTree;

  // file version
  saveMatrix("version", version, fileTree);

  // folders
  if(!sfmData.getRelativeFeaturesFolders().empty())
  {
    bpt::ptree featureFoldersTree;

    for(const std::string& featuresFolder : sfmData.getRelativeFeaturesFolders())
    {
      bpt::ptree featureFolderTree;
      featureFolderTree.put("", featuresFolder);
      featureFoldersTree.push_back(std::make_pair("", featureFolderTree));
    }

    fileTree.add_child("featuresFolders", featureFoldersTree);
  }

  if(!sfmData.getRelativeMatchesFolders().empty())
  {
    bpt::ptree matchingFoldersTree;

    for(const std::string& matchesFolder : sfmData.getRelativeMatchesFolders())
    {
      bpt::ptree matchingFolderTree;
      matchingFolderTree.put("", matchesFolder);
      matchingFoldersTree.push_back(std::make_pair("", matchingFolderTree));
    }

    fileTree.add_child("matchesFolders", matchingFoldersTree);
  }

  // views
  if(saveViews && !sfmData.GetViews().empty())
  {
    bpt::ptree viewsTree;

    for(const auto& viewPair : sfmData.GetViews())
      saveView("", *(viewPair.second), viewsTree);

    fileTree.add_child("views", viewsTree);
  }

  // intrinsics
  if(saveIntrinsics && !sfmData.GetIntrinsics().empty())
  {
    bpt::ptree intrinsicsTree;

    for(const auto& intrinsicPair : sfmData.GetIntrinsics())
      saveIntrinsic("", intrinsicPair.first, intrinsicPair.second, intrinsicsTree);

    fileTree.add_child("intrinsics", intrinsicsTree);
  }

  //extrinsics
  if(saveExtrinsics)
  {
    // poses
    if(!sfmData.GetPoses().empty())
    {
      bpt::ptree posesTree;

      for(const auto& posePair : sfmData.GetPoses())
      {
        bpt::ptree poseTree;

        poseTree.put("poseId", posePair.first);
        savePose3("pose", posePair.second, poseTree);
        posesTree.push_back(std::make_pair("", poseTree));
      }

      fileTree.add_child("poses", posesTree);
    }

    // rigs
    if(!sfmData.getRigs().empty())
    {
      bpt::ptree rigsTree;

      for(const auto& rigPair : sfmData.getRigs())
        saveRig("", rigPair.first, rigPair.second, rigsTree);

      fileTree.add_child("rigs", rigsTree);
    }
  }

  // structure
  if(saveStructure && !sfmData.GetLandmarks().empty())
  {
    bpt::ptree structureTree;

    for(const auto& structurePair : sfmData.GetLandmarks())
      saveLandmark("", structurePair.first, structurePair.second, structureTree);

    fileTree.add_child("structure", structureTree);
  }

  // control points
  if(saveControlPoints && !sfmData.GetControl_Points().empty())
  {
    bpt::ptree controlPointTree;

    for(const auto& controlPointPair : sfmData.GetControl_Points())
      saveLandmark("", controlPointPair.first, controlPointPair.second, controlPointTree);

    fileTree.add_child("controlPoints", controlPointTree);
  }

  // write the json file with the tree

  bpt::write_json(filename, fileTree);

  return true;
}

bool loadJSON(SfMData& sfmData, const std::string& filename, ESfMData partFlag, bool incompleteViews)
{
  Vec3 version;

  // load flags
  const bool loadViews = (partFlag & VIEWS) == VIEWS;
  const bool loadIntrinsics = (partFlag & INTRINSICS) == INTRINSICS;
  const bool loadExtrinsics = (partFlag & EXTRINSICS) == EXTRINSICS;
  const bool loadStructure = (partFlag & STRUCTURE) == STRUCTURE;
  const bool loadControlPoints = (partFlag & CONTROL_POINTS) == CONTROL_POINTS;

  // main tree
  bpt::ptree fileTree;

  // read the json file and initialize the tree
  bpt::read_json(filename, fileTree);

  // version
  loadMatrix("version", version, fileTree);

  // folders
  if(fileTree.count("featuresFolders"))
    for(bpt::ptree::value_type& featureFolderNode : fileTree.get_child("featuresFolders"))
      sfmData.addFeaturesFolder(featureFolderNode.second.get_value<std::string>());

  if(fileTree.count("matchesFolders"))
    for(bpt::ptree::value_type& matchingFolderNode : fileTree.get_child("matchesFolders"))
      sfmData.addMatchesFolder(matchingFolderNode.second.get_value<std::string>());

  // views
  if(loadViews && fileTree.count("views"))
  {
    Views& views = sfmData.GetViews();

    if(incompleteViews)
    {
      // store incomplete views in a vector
      std::vector<View> incompleteViews(fileTree.get_child("views").size());

      int viewIndex = 0;
      for(bpt::ptree::value_type &viewNode : fileTree.get_child("views"))
      {
        loadView(incompleteViews.at(viewIndex), viewNode.second);
        ++viewIndex;
      }

      // update incomplete views
      #pragma omp parallel for
      for(int i = 0; i < incompleteViews.size(); ++i)
        updateIncompleteView(incompleteViews.at(i));

      // copy complete views in the SfMData views map
      for(const View& view : incompleteViews)
        views.emplace(view.getViewId(), std::make_shared<View>(view));
    }
    else
    {
      // store directly in the SfMData views map
      for(bpt::ptree::value_type &viewNode : fileTree.get_child("views"))
      {
        View view;
        loadView(view, viewNode.second);
        views.emplace(view.getViewId(), std::make_shared<View>(view));
      }
    }
  }

  // intrinsics
  if(loadIntrinsics && fileTree.count("intrinsics"))
  {
    Intrinsics& intrinsics = sfmData.GetIntrinsics();

    for(bpt::ptree::value_type &intrinsicNode : fileTree.get_child("intrinsics"))
    {
      IndexT intrinsicId;
      std::shared_ptr<camera::IntrinsicBase> intrinsic;

      loadIntrinsic(intrinsicId, intrinsic, intrinsicNode.second);

      intrinsics.emplace(intrinsicId, intrinsic);
    }
  }

  // extrinsics
  if(loadExtrinsics)
  {
    // poses
    if(fileTree.count("poses"))
    {
      Poses& poses = sfmData.GetPoses();

      for(bpt::ptree::value_type &poseNode : fileTree.get_child("poses"))
      {
        bpt::ptree& poseTree = poseNode.second;
        geometry::Pose3 pose;

        loadPose3("pose", pose, poseTree);

        poses.emplace(poseTree.get<IndexT>("poseId"), pose);
      }
    }

    // rigs
    if(fileTree.count("rigs"))
    {
      Rigs& rigs = sfmData.getRigs();

      for(bpt::ptree::value_type &rigNode : fileTree.get_child("rigs"))
      {
        IndexT rigId;
        Rig rig;

        loadRig(rigId, rig, rigNode.second);

        rigs.emplace(rigId, rig);
      }
    }
  }

  // structure
  if(loadStructure && fileTree.count("structure"))
  {
    Landmarks& structure = sfmData.GetLandmarks();

    for(bpt::ptree::value_type &landmarkNode : fileTree.get_child("structure"))
    {
      IndexT landmarkId;
      Landmark landmark;

      loadLandmark(landmarkId, landmark, landmarkNode.second);

      structure.emplace(landmarkId, landmark);
    }
  }

  // control points
  if(loadControlPoints && fileTree.count("controlPoints"))
  {
    Landmarks& controlPoints = sfmData.GetControl_Points();

    for(bpt::ptree::value_type &landmarkNode : fileTree.get_child("controlPoints"))
    {
      IndexT landmarkId;
      Landmark landmark;

      loadLandmark(landmarkId, landmark, landmarkNode.second);

      controlPoints.emplace(landmarkId, landmark);
    }
  }

  return true;
}

} // namespace sfm
} // namespace aliceVision
