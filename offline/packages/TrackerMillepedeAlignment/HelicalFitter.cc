#include "HelicalFitter.h"

#include "Mille.h"

/// Tracking includes
#include <trackbase/TrkrDefs.h>                // for cluskey, getTrkrId, tpcId
#include <trackbase/TpcDefs.h>
#include <trackbase/MvtxDefs.h>
#include <trackbase/InttDefs.h>
#include <trackbase/TrkrClusterv3.h>   
#include <trackbase/TrkrClusterContainerv4.h>
#include <trackbase/TrkrClusterContainer.h>   
#include <trackbase/TrkrClusterCrossingAssoc.h>   
#include <trackbase/alignmentTransformationContainer.h>

#include <trackbase_historic/TrackSeed_v1.h>
#include <trackbase_historic/TrackSeedContainer_v1.h>
#include <trackbase_historic/SvtxTrackSeed_v1.h>
#include <trackbase_historic/SvtxVertex.h>     // for SvtxVertex
#include <trackbase_historic/SvtxVertexMap.h>
#include <trackbase_historic/SvtxTrack_v4.h>
#include <trackbase_historic/SvtxTrackMap_v2.h>
#include <trackbase_historic/SvtxAlignmentState_v1.h>
#include <trackbase_historic/SvtxAlignmentStateMap_v1.h>
#include <trackbase_historic/SvtxTrackState_v1.h>

#include <g4main/PHG4Hit.h>  // for PHG4Hit
#include <g4main/PHG4Particle.h>  // for PHG4Particle
#include <g4main/PHG4HitDefs.h>  // for keytype

#include <fun4all/Fun4AllReturnCodes.h>

#include <phool/phool.h>
#include <phool/PHCompositeNode.h>
#include <phool/getClass.h>

#include <TF1.h>
#include <TNtuple.h>
#include <TFile.h>

#include <climits>                            // for UINT_MAX
#include <iostream>                            // for operator<<, basic_ostream
#include <cmath>                              // for fabs, sqrt
#include <set>                                 // for _Rb_tree_const_iterator
#include <utility>                             // for pair
#include <memory>

using namespace std;

//____________________________________________________________________________..
HelicalFitter::HelicalFitter(const std::string &name):
  SubsysReco(name)
  , PHParameterInterface(name)
  , _mille(nullptr)
{
  InitializeParameters();
}

//____________________________________________________________________________..
HelicalFitter::~HelicalFitter()
{

}

//____________________________________________________________________________..
int HelicalFitter::InitRun(PHCompositeNode *topNode)
{
  UpdateParametersWithMacro();

   int ret = GetNodes(topNode);
  if (ret != Fun4AllReturnCodes::EVENT_OK) return ret;

  ret = CreateNodes(topNode);
  if(ret != Fun4AllReturnCodes::EVENT_OK) return ret;

  // Instantiate Mille and open output data file
  if(test_output)
    {
      _mille = new Mille(data_outfilename.c_str(), false);   // write text in data files, rather than binary, for debugging only
  }
 else
   {
     _mille = new Mille(data_outfilename.c_str()); 
   }

  // Write the steering file here, and add the data file path to it
  std::ofstream steering_file(steering_outfilename);
  steering_file << data_outfilename << std::endl;
  steering_file.close();

  if(make_ntuple)
    {
      fout = new TFile("HF_ntuple.root","recreate");
      ntp = new TNtuple("ntp","HF ntuple","event:trkid:layer:nsilicon:ntpc:nclus:trkrid:sector:side:subsurf:phi:glbl0:glbl1:glbl2:glbl3:glbl4:glbl5:sensx:sensy:sensz:normx:normy:normz:sensxideal:sensyideal:senszideal:normxideal:normyideal:normzideal:xglobideal:yglobideal:zglobideal:R:X0:Y0:Zs:Z0:xglob:yglob:zglob:xfit:yfit:zfit:pcax:pcay:pcaz:tangx:tangy:tangz:X:Y:fitX:fitY:dXdR:dXdX0:dXdY0:dXdZs:dXdZ0:dXdalpha:dXdbeta:dXdgamma:dXdx:dXdy:dXdz:dYdR:dYdX0:dYdY0:dYdZs:dYdZ0:dYdalpha:dYdbeta:dYdgamma:dYdx:dYdy:dYdz");
   }
 
  // print grouping setup to log file:
  std::cout << "HelicalFitter::InitRun: Surface groupings are mvtx " << mvtx_grp << " intt " << intt_grp << " tpc " << tpc_grp << " mms " << mms_grp << std::endl; 
  std::cout << " possible groupings are:" << std::endl
	    << " mvtx " 
	    << AlignmentDefs::mvtxGrp::snsr << "  " 
	    << AlignmentDefs::mvtxGrp::stv << "  " 
	    << AlignmentDefs::mvtxGrp::mvtxlyr << "  "
	    << AlignmentDefs::mvtxGrp::clamshl << "  " << std::endl
	    << " intt " 
	    << AlignmentDefs::inttGrp::chp << "  "
	    << AlignmentDefs::inttGrp::lad << "  "
	    << AlignmentDefs::inttGrp::inttlyr << "  "
	    << AlignmentDefs::inttGrp::inttbrl << "  " << std::endl
	    << " tpc " 
	    << AlignmentDefs::tpcGrp::htst << "  "
	    << AlignmentDefs::tpcGrp::sctr << "  "
	    << AlignmentDefs::tpcGrp::tp << "  " << std::endl
	    << " mms " 
	    << AlignmentDefs::mmsGrp::tl << "  "
	    << AlignmentDefs::mmsGrp::mm << "  " << std::endl;
 
  event=-1;

  return ret;
}

//_____________________________________________________________________
void HelicalFitter::SetDefaultParameters()
{


  return;
}

//____________________________________________________________________________..
int HelicalFitter::process_event(PHCompositeNode*)
{
  // _track_map_tpc contains the TPC seed track stubs
  // _track_map_silicon contains the silicon seed track stubs
  // _svtx_seed_map contains the combined silicon and tpc track seeds

  event++;

  if(Verbosity() > 0)
    cout << PHWHERE 
	 << " TPC seed map size " << _track_map_tpc->size() 
	 << " Silicon seed map size "  << _track_map_silicon->size() 
	 << endl;

  if(_track_map_silicon->size() == 0 && _track_map_tpc->size() == 0)
    return Fun4AllReturnCodes::EVENT_OK;

  // Decide whether we want to make a helical fit for silicon or TPC
  unsigned int maxtracks = 0; 
  unsigned int nsilicon = 0;
  unsigned int ntpc = 0;
  unsigned int nclus = 0;
  if(fittpc) { maxtracks =  _track_map_tpc->size();  }
  if(fitsilicon)  { maxtracks =  _track_map_silicon->size(); }
  for(unsigned int trackid = 0; trackid < maxtracks; ++trackid)
    {
      TrackSeed* tracklet = nullptr;
      if(fitsilicon) {  tracklet = _track_map_silicon->get(trackid); }
      else if(fittpc) {  tracklet = _track_map_tpc->get(trackid);	 }
      if(!tracklet) { trackid++;  continue; }

      std::vector<Acts::Vector3> global_vec;
      std::vector<TrkrDefs::cluskey> cluskey_vec;

      // Get a vector of cluster keys from the tracklet  
      getTrackletClusterList(tracklet, cluskey_vec);
      // store cluster global positions in a vector
      TrackFitUtils::getTrackletClusters(_tGeometry, _cluster_map, global_vec, cluskey_vec);   

      correctTpcGlobalPositions( global_vec, cluskey_vec);

      std::vector<float> fitpars =  TrackFitUtils::fitClusters(global_vec, cluskey_vec);       // do helical fit

      if(fitpars.size() == 0) continue;  // discard this track, not enough clusters to fit

      if(Verbosity() > 1)  
	{ std::cout << " Track " << trackid   << " radius " << fitpars[0] << " X0 " << fitpars[1]<< " Y0 " << fitpars[2]
		 << " zslope " << fitpars[3]  << " Z0 " << fitpars[4] << std::endl; }
      
      /// Create a track map for diagnostics
      SvtxTrack_v4 newTrack;
      if(fitsilicon) { newTrack.set_silicon_seed(tracklet); }
      else if(fittpc) {  newTrack.set_tpc_seed(tracklet); }
      
      // if a full track is requested, get the silicon clusters too and refit
      if(fittpc && fitfulltrack)
	{
	  // this associates silicon clusters and adds them to the vectors
	  ntpc = cluskey_vec.size();
	  nsilicon = TrackFitUtils::addSiliconClusters(fitpars, dca_cut, _tGeometry, _cluster_map, global_vec, cluskey_vec);
	  if(nsilicon < 3) continue;  // discard this TPC seed, did not get a good match to silicon
	  auto trackseed = std::make_unique<TrackSeed_v1>();
	  for(auto& ckey : cluskey_vec)
	    {
	      if(TrkrDefs::getTrkrId(ckey) == TrkrDefs::TrkrId::mvtxId or
		 TrkrDefs::getTrkrId(ckey) == TrkrDefs::TrkrId::inttId)
		{
		  trackseed->insert_cluster_key(ckey);
		}
	    }

	  newTrack.set_silicon_seed(trackseed.get());
	  
	  // fit the full track now
	  fitpars.clear();
	  fitpars =  TrackFitUtils::fitClusters(global_vec, cluskey_vec);       // do helical fit
	  if(fitpars.size() == 0) continue;  // discard this track, fit failed

	  if(Verbosity() > 1)  
	    { std::cout << " Full track " << trackid   << " radius " << fitpars[0] << " X0 " << fitpars[1]<< " Y0 " << fitpars[2]
					   << " zslope " << fitpars[3]  << " Z0 " << fitpars[4] << std::endl; }
	} 

      newTrack.set_crossing(tracklet->get_crossing());
      newTrack.set_id(trackid);
      /// use the track seed functions to help get the track trajectory values
      /// in the usual coordinates
  
      TrackSeed_v1 someseed;
      for(auto& ckey : cluskey_vec)
	{ someseed.insert_cluster_key(ckey); }
      someseed.set_qOverR(tracklet->get_charge() / fitpars[0]);
 
      someseed.set_X0(fitpars[1]);
      someseed.set_Y0(fitpars[2]);
      someseed.set_Z0(fitpars[4]);
      someseed.set_slope(fitpars[3]);

      newTrack.set_x(someseed.get_x());
      newTrack.set_y(someseed.get_y());
      newTrack.set_z(someseed.get_z());
      newTrack.set_px(someseed.get_px(_cluster_map,_tGeometry));
      newTrack.set_py(someseed.get_py(_cluster_map,_tGeometry));
      newTrack.set_pz(someseed.get_pz());
  
      newTrack.set_charge(tracklet->get_charge());
      SvtxAlignmentStateMap::StateVec statevec;
  
      nclus = ntpc+nsilicon;

      // some basic track quality requirements
      if(fittpc && ntpc < 35) continue;
      if((fitsilicon || fitfulltrack) && nsilicon < 5) continue;

      // get the residuals and derivatives for all clusters
      for(unsigned int ivec=0;ivec<global_vec.size(); ++ivec)
	{

	  auto global = global_vec[ivec];
	  auto cluskey = cluskey_vec[ivec];
	  auto cluster = _cluster_map->findCluster(cluskey);
	  if(!cluster) { continue;}

	  unsigned int trkrid = TrkrDefs::getTrkrId(cluskey);

	  // What we need now is to find the point on the surface at which the helix would intersect
	  // If we have that point, we can transform the fit back to local coords
	  // we have fitpars for the helix, and the cluster key - from which we get the surface

	  Surface surf = _tGeometry->maps().getSurface(cluskey, cluster);
	  Acts::Vector3 helix_pca(0,0,0);
	  Acts::Vector3 helix_tangent(0,0,0);
	  Acts::Vector3 fitpoint = get_helix_surface_intersection(surf, fitpars, global, helix_pca, helix_tangent);

	  // fitpoint is the point where the helical fit intersects the plane of the surface
	  // Now transform the helix fitpoint to local coordinates to compare with cluster local coordinates
	  Acts::Vector3 fitpoint_local = surf->transform(_tGeometry->geometry().getGeoContext()).inverse() * (fitpoint *  Acts::UnitConstants::cm);
	  fitpoint_local /= Acts::UnitConstants::cm;

	  auto xloc = cluster->getLocalX();  // in cm
	  auto zloc = cluster->getLocalY();	  
	  if(trkrid == TrkrDefs::tpcId) { zloc = convertTimeToZ(cluskey, cluster); }
	  //float yloc = 0.0;   // Because the fitpoint is on the surface, y will always be zero in local coordinates

	  Acts::Vector2 residual(xloc - fitpoint_local(0), zloc - fitpoint_local(1)); 

	  unsigned int layer = TrkrDefs::getLayer(cluskey_vec[ivec]);	  
	  float phi =  atan2(global(1), global(0));

	  SvtxTrackState_v1 svtxstate(fitpoint.norm());
	  svtxstate.set_x(fitpoint(0));
	  svtxstate.set_y(fitpoint(1));
	  svtxstate.set_z(fitpoint(2));
	  auto tangent = get_helix_tangent(fitpars, global);  
	  svtxstate.set_px(someseed.get_p() * tangent.second.x());
	  svtxstate.set_py(someseed.get_p() * tangent.second.y());
	  svtxstate.set_pz(someseed.get_p() * tangent.second.z());
	  newTrack.insert_state(&svtxstate);
	  
	  if(Verbosity() > 1) {
	  Acts::Vector3 loc_check =  surf->transform(_tGeometry->geometry().getGeoContext()).inverse() * (global *  Acts::UnitConstants::cm);
	  loc_check /= Acts::UnitConstants::cm;
	  std::cout << "    layer " << layer << std::endl
		    << " cluster global " << global(0) << " " << global(1) << " " << global(2) << std::endl
		    << " fitpoint " << fitpoint(0) << " " << fitpoint(1) << " " << fitpoint(2) << std::endl
		    << " fitpoint_local " << fitpoint_local(0) << " " << fitpoint_local(1) << " " << fitpoint_local(2) << std::endl  
		    << " cluster local x " << cluster->getLocalX() << " cluster local y " << cluster->getLocalY() << std::endl
		    << " cluster global to local x " << loc_check(0) << " local y " << loc_check(1) << "  local z " << loc_check(2) << std::endl
		    << " cluster local residual x " << residual(0) << " cluster local residual y " <<residual(1) << std::endl;
	  }


	  if(Verbosity() > 1) 
	    {
	      Acts::Transform3 transform =  surf->transform(_tGeometry->geometry().getGeoContext());
	      std::cout << "Transform is:" << std::endl;
	      std::cout <<  transform.matrix() << std::endl;
	      Acts::Vector3 loc_check =  surf->transform(_tGeometry->geometry().getGeoContext()).inverse() * (global *  Acts::UnitConstants::cm);
	      loc_check /= Acts::UnitConstants::cm;
	      unsigned int sector = TpcDefs::getSectorId(cluskey_vec[ivec]);	  
	      unsigned int side = TpcDefs::getSide(cluskey_vec[ivec]);	  
	      std::cout << "    layer " << layer << " sector " << sector << " side " << side << " subsurf " <<   cluster->getSubSurfKey() << std::endl
			<< " cluster global " << global(0) << " " << global(1) << " " << global(2) << std::endl
			<< " fitpoint " << fitpoint(0) << " " << fitpoint(1) << " " << fitpoint(2) << std::endl
			<< " fitpoint_local " << fitpoint_local(0) << " " << fitpoint_local(1) << " " << fitpoint_local(2) << std::endl  
			<< " cluster local x " << cluster->getLocalX() << " cluster local y " << cluster->getLocalY() << std::endl
			<< " cluster global to local x " << loc_check(0) << " local y " << loc_check(1) << "  local z " << loc_check(2) << std::endl
			<< " cluster local residual x " << residual(0) << " cluster local residual y " <<residual(1) << std::endl;
	    }
	  
	  // need standard deviation of measurements
	  Acts::Vector2 clus_sigma = getClusterError(cluster, cluskey, global);
	  if(isnan(clus_sigma(0)) || isnan(clus_sigma(1)))  { continue; }

	  int glbl_label[AlignmentDefs::NGL];
	  if(layer < 3)
	    {
	      AlignmentDefs::getMvtxGlobalLabels(surf, glbl_label, mvtx_grp);	      
	    }
	  else if(layer > 2 && layer < 7)
	    {
	      AlignmentDefs::getInttGlobalLabels(surf, glbl_label, intt_grp);	      
	      addGlobalConstraintIntt(glbl_label, surf); 
	    }
	  else if (layer < 55)
	    {
	      AlignmentDefs::getTpcGlobalLabels(surf, cluskey, glbl_label, tpc_grp);
	      /*
	      unsigned int sector = TpcDefs::getSectorId(cluskey_vec[ivec]);	  
	      unsigned int side = TpcDefs::getSide(cluskey_vec[ivec]);	  
	      Acts::Vector3 center =  surf->center(_tGeometry->geometry().getGeoContext()) * 0.1;  // convert to cm
	      Acts::GeometryIdentifier id = surf->geometryId();
	      unsigned int region = AlignmentDefs::getTpcRegion(layer);
	      TrkrDefs::hitsetkey hitsetkey = TrkrDefs::getHitSetKeyFromClusKey(cluskey);
	      std::cout << std::endl << "     layer " << layer << " hitsetkey " << hitsetkey << " sector " << sector 
			<< " region " << region << " side " << side 
			<< " sensor center " << center[0] << "  " << center[1] << "  " << center[2] 
			<< " phi " << atan2(center[1],center[0])* 180.0/M_PI << std::endl;
	      std::cout << "     global labels: " << glbl_label[3] << "  " << glbl_label[4] << "  " << glbl_label[5] << std::endl;
	      std::cout << " TPC surface GeometryIdentifier " << id << std::endl;
	      std::cout << " subsurf key from cluster " << cluster->getSubSurfKey() << std::endl;
	      std::cout << " transform: " << std::endl << surf->transform(_tGeometry->geometry().getGeoContext()).matrix() << std::endl;
	      // get the local parameters using the ideal transforms
	      alignmentTransformationContainer::use_alignment = false;
	      Acts::Vector3 ideal_center =  surf->center(_tGeometry->geometry().getGeoContext()) * 0.1;  // convert to cm
	      std::cout << " ideal sensor center " << ideal_center[0] << "  " << ideal_center[1] << "  " << ideal_center[2] << std::endl;
	      std::cout << " ideal transform: " << std::endl << surf->transform(_tGeometry->geometry().getGeoContext()).matrix() << std::endl;
	      alignmentTransformationContainer::use_alignment = true;
	      */
	    }
	  else
	    {
	      continue;
	    }

	  // These derivatives are for the local parameters
	  float lcl_derivativeX[AlignmentDefs::NLC];
	  float lcl_derivativeY[AlignmentDefs::NLC];

	  getLocalDerivativesXY(surf, global, fitpars, lcl_derivativeX, lcl_derivativeY, layer);

	  // The global derivs dimensions are [alpha/beta/gamma](x/y/z)
	  float glbl_derivativeX[AlignmentDefs::NGL];
	  float glbl_derivativeY[AlignmentDefs::NGL];
	  getGlobalDerivativesXY(surf, global, fitpoint, fitpars, glbl_derivativeX, glbl_derivativeY, layer);

	  auto alignmentstate = std::make_unique<SvtxAlignmentState_v1>();
	  alignmentstate->set_residual(residual);
	  alignmentstate->set_cluster_key(cluskey);
	  SvtxAlignmentState::GlobalMatrix svtxglob = 
	    SvtxAlignmentState::GlobalMatrix::Zero();
	  SvtxAlignmentState::LocalMatrix svtxloc = 
	    SvtxAlignmentState::LocalMatrix::Zero();
	  for(int i=0; i<AlignmentDefs::NLC; i++)
	    {
	      svtxloc(0,i) = lcl_derivativeX[i];
	      svtxloc(1,i) = lcl_derivativeY[i];
	    }
	  for(int i=0; i<AlignmentDefs::NGL; i++)
	    {
	      svtxglob(0,i) = glbl_derivativeX[i];
	      svtxglob(1,i) = glbl_derivativeY[i];
	    }

	  alignmentstate->set_local_derivative_matrix(svtxloc);
	  alignmentstate->set_global_derivative_matrix(svtxglob);
	  statevec.push_back(alignmentstate.release());
	  
	  for(unsigned int i = 0; i < AlignmentDefs::NGL; ++i) 
	    {
	      if( is_layer_param_fixed(layer, i) || is_layer_fixed(layer) )
		{
		  glbl_derivativeX[i] = 0;
		  glbl_derivativeY[i] = 0;
		}

	      if(trkrid == TrkrDefs::tpcId)
		{
		  unsigned int sector = TpcDefs::getSectorId(cluskey_vec[ivec]);	  
		  unsigned int side = TpcDefs::getSide(cluskey_vec[ivec]);	  
		  if(is_tpc_sector_fixed(layer, sector, side))
		    {
		      glbl_derivativeX[i] = 0;
		      glbl_derivativeY[i] = 0;
		    }
		}
	    }

	  // Add the measurement separately for each coordinate direction to Mille
	  // set the derivatives non-zero only for parameters we want to be optimized
	  // local parameter numbering is arbitrary:
	  float errinf = 1.0;

	  if(_layerMisalignment.find(layer) != _layerMisalignment.end())
	    {
	      errinf = _layerMisalignment.find(layer)->second;
	    }

	  if(make_ntuple)
	    {
	      // get the local parameters using the ideal transforms
	      alignmentTransformationContainer::use_alignment = false;
	      Acts::Vector3 ideal_center =  surf->center(_tGeometry->geometry().getGeoContext()) * 0.1;  
	      Acts::Vector3 ideal_norm =  -surf->normal(_tGeometry->geometry().getGeoContext());  
	      Acts::Vector3 ideal_local(xloc, zloc, 0.0); // cm
	      Acts::Vector3 ideal_glob =  surf->transform(_tGeometry->geometry().getGeoContext())*(ideal_local * Acts::UnitConstants::cm);
	      ideal_glob /= Acts::UnitConstants::cm;
	      alignmentTransformationContainer::use_alignment = true;

	      Acts::Vector3 sensorCenter      = surf->center(_tGeometry->geometry().getGeoContext()) * 0.1; // cm
	      Acts::Vector3 sensorNormal    = -surf->normal(_tGeometry->geometry().getGeoContext());
	      unsigned int sector = TpcDefs::getSectorId(cluskey_vec[ivec]);	  
	      unsigned int side = TpcDefs::getSide(cluskey_vec[ivec]);	  	      
	      unsigned int subsurf = cluster->getSubSurfKey();
	      if(layer < 3)
		{
		  sector = MvtxDefs::getStaveId(cluskey_vec[ivec]);
		  subsurf = MvtxDefs::getChipId(cluskey_vec[ivec]);
		}
	      else if(layer >2 && layer < 7)
		{
		  sector = InttDefs::getLadderPhiId(cluskey_vec[ivec]);
		  subsurf = InttDefs::getLadderZId(cluskey_vec[ivec]);
		}
	      float ntp_data[75] = {
		(float) event, (float) trackid,
		(float) layer, (float) nsilicon, (float) ntpc, (float) nclus, (float) trkrid,  (float) sector,  (float) side,
		(float) subsurf, phi,
		(float) glbl_label[0], (float) glbl_label[1], (float) glbl_label[2], (float) glbl_label[3], (float) glbl_label[4], (float) glbl_label[5], 
		(float) sensorCenter(0), (float) sensorCenter(1), (float) sensorCenter(2),
		(float) sensorNormal(0), (float) sensorNormal(1), (float) sensorNormal(2),
		(float) ideal_center(0), (float) ideal_center(1), (float) ideal_center(2),
		(float) ideal_norm(0), (float) ideal_norm(1), (float) ideal_norm(2),
		(float) ideal_glob(0), (float) ideal_glob(1), (float) ideal_glob(2),
		(float) fitpars[0], (float) fitpars[1], (float) fitpars[2], (float) fitpars[3], (float) fitpars[4], 
		(float) global(0), (float) global(1), (float) global(2),
		(float) fitpoint(0), (float) fitpoint(1), (float) fitpoint(2), 
		(float) helix_pca(0), (float) helix_pca(1), (float) helix_pca(2),
		(float) helix_tangent(0), (float) helix_tangent(1), (float) helix_tangent(2),
		xloc,zloc, (float) fitpoint_local(0), (float) fitpoint_local(1), 
		lcl_derivativeX[0],lcl_derivativeX[1],lcl_derivativeX[2],lcl_derivativeX[3],lcl_derivativeX[4],
		glbl_derivativeX[0],glbl_derivativeX[1],glbl_derivativeX[2],glbl_derivativeX[3],glbl_derivativeX[4],glbl_derivativeX[5],
		lcl_derivativeY[0],lcl_derivativeY[1],lcl_derivativeY[2],lcl_derivativeY[3],lcl_derivativeY[4],
		glbl_derivativeY[0],glbl_derivativeY[1],glbl_derivativeY[2],glbl_derivativeY[3],glbl_derivativeY[4],glbl_derivativeY[5] };

	      ntp->Fill(ntp_data);

	      if(Verbosity() > 2)
		{
		  for(int i=0;i<34;++i)
		    {
		      std::cout << ntp_data[i] << "  " ;
		    }
		  std::cout << std::endl;
		}
	    };
	  
	  if( !isnan(residual(0)) && clus_sigma(0) < 1.0)  // discards crazy clusters
	    { 
	      _mille->mille(AlignmentDefs::NLC, lcl_derivativeX, AlignmentDefs::NGL, glbl_derivativeX, glbl_label, residual(0), errinf*clus_sigma(0));
	    }

	  if(!isnan(residual(1)) && clus_sigma(1) < 1.0 && trkrid != TrkrDefs::inttId)
	    {
	      _mille->mille(AlignmentDefs::NLC, lcl_derivativeY, AlignmentDefs::NGL, glbl_derivativeY, glbl_label, residual(1), errinf*clus_sigma(1));
	    }
	}
      
      m_alignmentmap->insertWithKey(newTrack.get_id(), statevec);
      m_trackmap->insertWithKey(&newTrack, trackid);
      // close out this track
      _mille->end();
      
    }  // end loop over tracks
  
  return Fun4AllReturnCodes::EVENT_OK;
}

void HelicalFitter::addGlobalConstraintIntt(int glbl_label[6], Surface surf)
{
  // Add entry for the INTT dx and dy global labels to the constraints file

  // use dx translation parameter as the key
  int label_dx = glbl_label[3];
  int label_dy = glbl_label[4];

  // Does this constraint exist already?
  if(InttConstraints.find(label_dx) != InttConstraints.end())
    {
      return;
    }

  // add it to the map
  // we need label_dx, label_dy, sensor x and sensor y
  alignmentTransformationContainer::use_alignment = false;
  Acts::Vector3 sensorCenter      = surf->center(_tGeometry->geometry().getGeoContext()) * 0.1;  // convert to cm
  alignmentTransformationContainer::use_alignment = true;
  /*
  float phi = atan2(sensorCenter(1), sensorCenter(0));
  float cosphi = cos(phi);
  float sinphi = sin(phi);
  std::cout << "  label_dx " << label_dx << " sensorCenter(0) " << sensorCenter(0) << " cosphi " << cosphi << std::endl;
  std::cout << "  label_dy " << label_dy << " sensorCenter(1) " << sensorCenter(1) << " sinphi " << sinphi << std::endl;
  std::cout << "    dx/dy " << -sensorCenter(1)/sensorCenter(0) << "  or  " << -sinphi/cosphi << std::endl;
  */
  std::pair<int, float> dx_x = std::make_pair(label_dx, sensorCenter(0));
  std::pair<int, float> dy_y = std::make_pair(label_dy, sensorCenter(1));
  auto constraint = std::make_pair(dx_x, dy_y);
  InttConstraints.insert(std::make_pair(label_dx, constraint));

  return;
}

Acts::Vector3 HelicalFitter::get_helix_surface_intersection(Surface surf, std::vector<float>& fitpars, Acts::Vector3 global)
{
  // we want the point where the helix intersects the plane of the surface

  // get the plane of the surface
  Acts::Vector3 sensorCenter      = surf->center(_tGeometry->geometry().getGeoContext()) * 0.1;  // convert to cm
  Acts::Vector3 sensorNormal    = -surf->normal(_tGeometry->geometry().getGeoContext());
  sensorNormal /= sensorNormal.norm();

  // there are analytic solutions for a line-plane intersection.
  // to use this, need to get the vector tangent to the helix near the measurement and a point on it.
  std::pair<Acts::Vector3, Acts::Vector3> line =  get_helix_tangent(fitpars, global);
  Acts::Vector3 pca = line.first;
  Acts::Vector3 tangent = line.second;

  //  std::cout << "   pca: "  << pca(0) << "  " << pca(1) << "  " << pca(2) << "  " << std::endl;

  Acts::Vector3 intersection = get_line_plane_intersection(pca, tangent, sensorCenter, sensorNormal);

  return intersection;
}

Acts::Vector3 HelicalFitter::get_helix_surface_intersection(Surface surf, std::vector<float>& fitpars, Acts::Vector3 global, Acts::Vector3& pca, Acts::Vector3& tangent)
{
  // we want the point where the helix intersects the plane of the surface

  // get the plane of the surface
  Acts::Vector3 sensorCenter      = surf->center(_tGeometry->geometry().getGeoContext()) * 0.1;  // convert to cm
  Acts::Vector3 sensorNormal    = -surf->normal(_tGeometry->geometry().getGeoContext());
  sensorNormal /= sensorNormal.norm();

  // there are analytic solutions for a line-plane intersection.
  // to use this, need to get the vector tangent to the helix near the measurement and a point on it.
  std::pair<Acts::Vector3, Acts::Vector3> line =  get_helix_tangent(fitpars, global);
  // Acts::Vector3 pca = line.first;
  // Acts::Vector3 tangent = line.second;
  pca = line.first;
  tangent = line.second;

  //  std::cout << "   pca: "  << pca(0) << "  " << pca(1) << "  " << pca(2) << "  " << std::endl;

  Acts::Vector3 intersection = get_line_plane_intersection(pca, tangent, sensorCenter, sensorNormal);

  return intersection;
}

Acts::Vector3 HelicalFitter::get_line_plane_intersection(Acts::Vector3 PCA, Acts::Vector3 tangent, Acts::Vector3 sensor_center, Acts::Vector3 sensor_normal)
{
  // get the intersection of the line made by PCA and tangent with the plane of the sensor

  // For a point on the line
  // p = PCA + d * tangent;
  // for a point on the plane
  // (p - sensor_center).sensor_normal = 0

 // The solution is:
  float d = (sensor_center - PCA).dot(sensor_normal) / tangent.dot(sensor_normal);
  Acts::Vector3 intersection = PCA + d * tangent;
  /*
  std::cout << "   intersection: " << intersection(0) << "  "  << intersection(1) << "  "  << intersection(2) << "  " << std::endl;
  std::cout << "        sensor_center: " << sensor_center(0) << "  " << sensor_center(1) << "  " << sensor_center(2) << "  " << std::endl;
  std::cout << "        sensor_normal: " << sensor_normal(0) << "  " << sensor_normal(1) << "  " << sensor_normal(2) << "  " << std::endl;
  */
  return intersection;
}


std::pair<Acts::Vector3, Acts::Vector3> HelicalFitter::get_helix_tangent(const std::vector<float>& fitpars, Acts::Vector3 global)
{
  // no analytic solution for the coordinates of the closest approach of a helix to a point
  // Instead, we get the PCA in x and y to the circle, and the PCA in z to the z vs R line at the R of the PCA 

  float radius = fitpars[0];
  float x0 = fitpars[1];
  float y0 = fitpars[2];  
  float zslope = fitpars[3];
  float z0 = fitpars[4];

  Acts::Vector2 pca_circle = get_circle_point_pca(radius, x0, y0, global);

  // The radius of the PCA determines the z position:
  float pca_circle_radius = pca_circle.norm();  // radius of the PCA of the circle to the point
  float pca_z = pca_circle_radius * zslope + z0;
  Acts::Vector3 pca(pca_circle(0), pca_circle(1), pca_z);

  // now we want a second point on the helix so we can get a local straight line approximation to the track
  // Get the angle of the PCA relative to the fitted circle center
  float angle_pca = atan2(pca_circle(1) - y0, pca_circle(0) - x0);
  // calculate coords of a point at a slightly larger angle
  float d_angle = 0.005;
  float newx = radius * cos(angle_pca + d_angle) + x0;
  float newy = radius * sin(angle_pca + d_angle) + y0;
  float newz = sqrt(newx*newx+newy*newy) * zslope + z0;
  Acts::Vector3 second_point_pca(newx, newy, newz);

  // pca and second_point_pca define a straight line approximation to the track
  Acts::Vector3 tangent = (second_point_pca - pca) /  (second_point_pca - pca).norm();

 // get the PCA of the cluster to that line
  Acts::Vector3 final_pca = getPCALinePoint(global, tangent, pca);

  if(Verbosity() > 2)
    {
      // different method for checking:
      // project the circle PCA vector an additional small amount and find the helix PCA to that point 
      float projection = 0.25;  // cm
      Acts::Vector3 second_point = pca + projection * pca/pca.norm();
      Acts::Vector2 second_point_pca_circle = get_circle_point_pca(radius, x0, y0, second_point);
      float second_point_pca_z = second_point_pca_circle.norm() * zslope + z0;
      Acts::Vector3 second_point_pca2(second_point_pca_circle(0), second_point_pca_circle(1), second_point_pca_z);
      Acts::Vector3 tangent2 = (second_point_pca2 - pca) /  (second_point_pca2 - pca).norm();
      Acts::Vector3 final_pca2 = getPCALinePoint(global, tangent2, pca);
    
      std::cout << " get_helix_tangent: getting tangent at angle_pca: " << angle_pca * 180.0 / M_PI << std::endl 
		<< " original first point pca                      " << pca(0) << "  " << pca(1) << "  " << pca(2) << std::endl
		<< " original second point pca  " << second_point_pca(0) << "  " << second_point_pca(1) << "  " << second_point_pca(2) << std::endl
		<< " original tangent " << tangent(0) << "  " << tangent(1) << "  " << tangent(2) << std::endl	
		<< " original final pca from line " << final_pca(0) << "  " << final_pca(1) << "  " << final_pca(2) << std::endl;

      if(Verbosity() > 3)
	{
	  std::cout	<< "    Check: 2nd point pca meth 2 "<< second_point_pca2(0)<< "  "<< second_point_pca2(1) << "  "<< second_point_pca2(2) << std::endl
			<< "    check tangent " << tangent2(0) << "  " << tangent2(1) << "  " << tangent2(2) << std::endl	
			<< "    check final pca from line " << final_pca2(0) << "  " << final_pca2(1) << "  " << final_pca2(2) 
			<< std::endl;
	}
    }


  std::pair<Acts::Vector3, Acts::Vector3> line = std::make_pair(final_pca, tangent);

  return line;
}
  
int HelicalFitter::End(PHCompositeNode* )
{
  // closes output file in destructor
  delete _mille;

  if(make_ntuple)
    {
     fout->Write();
     fout->Close();
    }

  ofstream fconstraint("mille_global_constraints.txt");
  for (auto it = InttConstraints.begin(); it != InttConstraints.end(); ++it)
    {
      auto xpair = it->second.first;
      auto ypair = it->second.second;

      fconstraint << "Constraint 0.0" << std::endl;
      fconstraint << xpair.first << "  " << xpair.second << std::endl;
      fconstraint << ypair.first << "  " << ypair.second << std::endl;
    }
  fconstraint.close();

  return Fun4AllReturnCodes::EVENT_OK;
}
int HelicalFitter::CreateNodes(PHCompositeNode* topNode)
{
  PHNodeIterator iter(topNode);
  
  PHCompositeNode *dstNode = dynamic_cast<PHCompositeNode*>(iter.findFirst("PHCompositeNode", "DST"));

  if (!dstNode)
  {
    std::cerr << "DST node is missing, quitting" << std::endl;
    throw std::runtime_error("Failed to find DST node in PHActsTrkFitter::createNodes");
  }
  
  PHNodeIterator dstIter(topNode);
  PHCompositeNode *svtxNode = dynamic_cast<PHCompositeNode *>(dstIter.findFirst("PHCompositeNode", "SVTX"));
  if (!svtxNode)
  {
    svtxNode = new PHCompositeNode("SVTX");
    dstNode->addNode(svtxNode);
  }
  
  m_trackmap = findNode::getClass<SvtxTrackMap>(topNode, "HelicalFitterTrackMap");
  if(!m_trackmap)
    {
      m_trackmap = new SvtxTrackMap_v2;
      PHIODataNode<PHObject> *node = new PHIODataNode<PHObject>(m_trackmap,"HelicalFitterTrackMap","PHObject");
      svtxNode->addNode(node);
    }

  m_alignmentmap = findNode::getClass<SvtxAlignmentStateMap>(topNode,"HelicalFitterAlignmentStateMap");
  if(!m_alignmentmap)
    {
      m_alignmentmap = new SvtxAlignmentStateMap_v1;
      PHIODataNode<PHObject> *node = new PHIODataNode<PHObject>(m_alignmentmap,"HelicalFitterAlignmentStateMap","PHObject");
      svtxNode->addNode(node);
    }

  return Fun4AllReturnCodes::EVENT_OK;
}
int  HelicalFitter::GetNodes(PHCompositeNode* topNode)
{
  //---------------------------------
  // Get additional objects off the Node Tree
  //---------------------------------

  _track_map_silicon = findNode::getClass<TrackSeedContainer>(topNode, _silicon_track_map_name);
  if (!_track_map_silicon)
  {
    cerr << PHWHERE << " ERROR: Can't find SiliconTrackSeedContainer " << endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  _track_map_tpc = findNode::getClass<TrackSeedContainer>(topNode, _track_map_name);
  if (!_track_map_tpc)
  {
    cerr << PHWHERE << " ERROR: Can't find " << _track_map_name.c_str() << endl;
    return Fun4AllReturnCodes::ABORTEVENT;
  }

  _cluster_map = findNode::getClass<TrkrClusterContainer>(topNode, "TRKR_CLUSTER");
  if (!_cluster_map)
    {
      std::cout << PHWHERE << " ERROR: Can't find node TRKR_CLUSTER" << std::endl;
      return Fun4AllReturnCodes::ABORTEVENT;
    }

  _tGeometry = findNode::getClass<ActsGeometry>(topNode,"ActsGeometry");
  if(!_tGeometry)
    {
      std::cout << PHWHERE << "Error, can't find acts tracking geometry" << std::endl;
      return Fun4AllReturnCodes::ABORTEVENT;
    }
  
  return Fun4AllReturnCodes::EVENT_OK;
} 

Acts::Vector3 HelicalFitter::get_helix_pca(std::vector<float>& fitpars, Acts::Vector3 global)
{
  return TrackFitUtils::get_helix_pca(fitpars, global);
}


Acts::Vector3 HelicalFitter::getPCALinePoint(Acts::Vector3 global, Acts::Vector3 tangent, Acts::Vector3 posref)
{
  // Approximate track with a straight line consisting of the state position posref and the vector (px,py,pz)   

  // The position of the closest point on the line to global is:
  // posref + projection of difference between the point and posref on the tangent vector
  Acts::Vector3 pca = posref + ( (global - posref).dot(tangent) ) * tangent;

  return pca;
}

Acts::Vector2 HelicalFitter::get_circle_point_pca(float radius, float x0, float y0, Acts::Vector3 global)
{
  // get the PCA of a cluster (x,y) position to a circle
  // draw a line from the origin of the circle to the point
  // the intersection of the line with the circle is at the distance radius from the origin along that line 

  Acts::Vector2 origin(x0, y0);
  Acts::Vector2 point(global(0), global(1));

  Acts::Vector2 pca = origin + radius * (point - origin) / (point - origin).norm();

  return pca;
}

float HelicalFitter::convertTimeToZ(TrkrDefs::cluskey cluster_key, TrkrCluster *cluster)
{
  // must convert local Y from cluster average time of arival to local cluster z position
  double drift_velocity = _tGeometry->get_drift_velocity();
  double zdriftlength = cluster->getLocalY() * drift_velocity;
  double surfCenterZ = 52.89; // 52.89 is where G4 thinks the surface center is
  double zloc = surfCenterZ - zdriftlength;   // converts z drift length to local z position in the TPC in north
  unsigned int side = TpcDefs::getSide(cluster_key);
  if(side == 0) zloc = -zloc;
  float z = zloc;  // in cm
 
  return z; 
}

void HelicalFitter::makeTpcGlobalCorrections(TrkrDefs::cluskey cluster_key, short int crossing, Acts::Vector3& global)
{
  // make all corrections to global position of TPC cluster
  unsigned int side = TpcDefs::getSide(cluster_key);
  float z = m_clusterCrossingCorrection.correctZ(global[2], side, crossing);
  global[2] = z;
  
  // apply distortion corrections
  if(_dcc_static) { global = _distortionCorrection.get_corrected_position( global, _dcc_static ); }
  if(_dcc_average) { global = _distortionCorrection.get_corrected_position( global, _dcc_average ); }
  if(_dcc_fluctuation) { global = _distortionCorrection.get_corrected_position( global, _dcc_fluctuation ); }
}


// this method is an interface to TrackFitUtils
void HelicalFitter::getTrackletClusters(TrackSeed *tracklet, std::vector<Acts::Vector3>& global_vec, std::vector<TrkrDefs::cluskey>& cluskey_vec)
{
  getTrackletClusterList(tracklet, cluskey_vec);
  // store cluster global positions in a vector
  TrackFitUtils::getTrackletClusters(_tGeometry, _cluster_map, global_vec, cluskey_vec);   
}

void HelicalFitter::getTrackletClusterList(TrackSeed *tracklet, std::vector<TrkrDefs::cluskey>& cluskey_vec)
{
  for (auto clusIter = tracklet->begin_cluster_keys();
       clusIter != tracklet->end_cluster_keys();
       ++clusIter)
    {
      auto key = *clusIter;
      auto cluster = _cluster_map->findCluster(key);
      if(!cluster)
	{
	  std::cout << "Failed to get cluster with key " << key << std::endl;
	  continue;
	}	  
      
      /// Make a safety check for clusters that couldn't be attached to a surface
      auto surf = _tGeometry->maps().getSurface(key, cluster);
      if(!surf)  { continue; }

      // drop some bad layers in the TPC completely
      unsigned int layer = TrkrDefs::getLayer(key);
      if(layer == 7 || layer == 22 || layer == 23 || layer == 38 || layer == 39) {continue;}

      cluskey_vec.push_back(key);
      
    } // end loop over clusters for this track 
}

std::vector<float> HelicalFitter::fitClusters(std::vector<Acts::Vector3>& global_vec, std::vector<TrkrDefs::cluskey> cluskey_vec)
{
  return TrackFitUtils::fitClusters(global_vec, cluskey_vec);       // do helical fit
}

Acts::Vector2 HelicalFitter::getClusterError(TrkrCluster *cluster, TrkrDefs::cluskey cluskey, Acts::Vector3& global)
{
  Acts::Vector2 clus_sigma(0,0);

  if(_cluster_version==3)
    {
      clus_sigma(1) = cluster->getZError();
      clus_sigma(0) = cluster->getRPhiError();
    }
  else if(_cluster_version==4)
    {
      double clusRadius = sqrt(global[0]*global[0] + global[1]*global[1]);
      auto para_errors = _ClusErrPara.get_simple_cluster_error(cluster,clusRadius,cluskey);
      float exy2 = para_errors.first * Acts::UnitConstants::cm2;
      float ez2 = para_errors.second * Acts::UnitConstants::cm2;
      clus_sigma(1) = sqrt(ez2);
      clus_sigma(0) = sqrt(exy2);
    }
  else if(_cluster_version == 5)
    {
      double clusRadius = sqrt(global[0]*global[0] + global[1]*global[1]);
      TrkrClusterv5* clusterv5 = dynamic_cast<TrkrClusterv5*>(cluster);
      auto para_errors = _ClusErrPara.get_clusterv5_modified_error(clusterv5,clusRadius,cluskey);
      double phierror = sqrt(para_errors.first);
      double zerror = sqrt(para_errors.second);
      clus_sigma(1) = zerror;
      clus_sigma(0) = phierror;
    }

  return clus_sigma; 
}

// new one
void HelicalFitter::getLocalDerivativesXY(Surface surf, Acts::Vector3 global, const std::vector<float>& fitpars, float lcl_derivativeX[5], float lcl_derivativeY[5], unsigned int layer)
{
  // Calculate the derivatives of the residual wrt the track parameters numerically
  std::vector<float> temp_fitpars;

  std::vector<float> fitpars_delta;
  fitpars_delta.push_back(0.1);   // radius, cm
  fitpars_delta.push_back(0.1);   // X0, cm
  fitpars_delta.push_back(0.1);   // Y0, cm
  fitpars_delta.push_back(0.1);   // zslope, cm
  fitpars_delta.push_back(0.1);   // Z0, cm

  for(unsigned int ip = 0; ip < fitpars.size(); ++ip)
    {
      temp_fitpars.push_back(fitpars[ip]);
    }

  // calculate projX and projY vectors once for the optimum fit parameters
  if(Verbosity() > 1) std::cout << "Call get_helix_tangent for best fit fitpars" << std::endl;
  std::pair<Acts::Vector3, Acts::Vector3> tangent = get_helix_tangent(fitpars, global);

  Acts::Vector3 projX(0,0,0), projY(0,0,0);
  get_projectionXY(surf, tangent, projX, projY);
  //  std::cout << "  projX "  << std::endl << projX << std::endl;
  //  std::cout << "  projY "  << std::endl << projY << std::endl;

  Acts::Vector3 intersection = get_helix_surface_intersection(surf, temp_fitpars, global);

  // loop over the track fit parameters
  for(unsigned int ip = 0; ip < fitpars.size(); ++ip)
    {
      Acts::Vector3 intersection_delta[2];
      for(int ipm = 0; ipm < 2; ++ipm)
	{
	  temp_fitpars[ip] = fitpars[ip];  // reset to best fit value
	  float deltapm = pow(-1.0, ipm);
	  temp_fitpars[ip] += deltapm * fitpars_delta[ip];

	  if(Verbosity() > 1)
	    {
	      std::cout << "Layer " << layer << " local parameter " << ip << " with ipm " << ipm << " deltapm " << deltapm << " :" << std::endl; 
	    }
	  Acts::Vector3 temp_intersection = get_helix_surface_intersection(surf, temp_fitpars, global);
	  intersection_delta[ipm] = temp_intersection - intersection;
	  if(Verbosity() > 1)
	    {
	      std::cout << " intersection " << intersection(0) << "  " << intersection(1) << "  " << intersection(2) << std::endl;
	      std::cout << " temp_intersection " << temp_intersection(0) << "  "<< temp_intersection(1) << "  "<< temp_intersection(2)<< std::endl;
	      std::cout << " intersection_delta " << intersection_delta[ipm](0) << "  " << intersection_delta[ipm](1) << "  " << intersection_delta[ipm](2) << std::endl;
	      std::cout << " intersection_delta / fitpars_delta = " <<  (intersection_delta[ipm] / fitpars_delta[ip])(0) << "  " <<  (intersection_delta[ipm] / fitpars_delta[ip])(1) << "  " <<  (intersection_delta[ipm] / fitpars_delta[ip])(2) << std::endl;;
	    }
	}
      Acts::Vector3 average_intersection_delta = (intersection_delta[0] - intersection_delta[1]) / (2 * fitpars_delta[ip]);
      // convert to delta-intersection / delta-parameter
      //      intersection_delta /= fitpars_delta[ip];

      if(Verbosity() > 1)
	{std::cout << " average_intersection_delta / delta " << average_intersection_delta(0) << "  " << average_intersection_delta(1) << "  " << average_intersection_delta(2) << std::endl;}

      // calculate the change in fit for X and Y 
      // - note negative sign from ATLAS paper is dropped here because mille wants the derivative of the fit, not the derivative of the residual
      lcl_derivativeX[ip] = average_intersection_delta.dot(projX);
      lcl_derivativeY[ip] = average_intersection_delta.dot(projY);
      if(Verbosity() > 1)
	{std::cout << " ip " << ip << "  derivativeX " << lcl_derivativeX[ip] << "  " << " derivativeY " << lcl_derivativeY[ip] << std::endl;}

      temp_fitpars[ip] = fitpars[ip];
    }
}

void HelicalFitter::getGlobalDerivativesXY(Surface surf, Acts::Vector3 global, Acts::Vector3 fitpoint, const std::vector<float>& fitpars, float glbl_derivativeX[6], float glbl_derivativeY[6], unsigned int layer)
{
  Acts::Vector3 unitx(1, 0, 0);
  Acts::Vector3 unity(0, 1, 0);
  Acts::Vector3 unitz(0, 0, 1);

  // calculate projX and projY vectors once for the optimum fit parameters
  std::pair<Acts::Vector3, Acts::Vector3> tangent = get_helix_tangent(fitpars, global);  // should this be global, not fitpoint?

  Acts::Vector3 projX(0,0,0), projY(0,0,0);
  get_projectionXY(surf, tangent, projX, projY);

  // translations

  glbl_derivativeX[3] = unitx.dot(projX);
  glbl_derivativeX[4] = unity.dot(projX);
  glbl_derivativeX[5] = unitz.dot(projX);

  glbl_derivativeY[3] = unitx.dot(projY);
  glbl_derivativeY[4] = unity.dot(projY);
  glbl_derivativeY[5] = unitz.dot(projY);

  // rotations

  // need center of sensor to intersection point
  Acts::Vector3 sensorCenter      = surf->center(_tGeometry->geometry().getGeoContext()) / Acts::UnitConstants::cm;  // convert to cm
  Acts::Vector3 OM = fitpoint - sensorCenter;

  glbl_derivativeX[0] = (unitx.cross(OM)).dot(projX);
  glbl_derivativeX[1] = (unity.cross(OM)).dot(projX);
  glbl_derivativeX[2] = (unitz.cross(OM)).dot(projX);

  glbl_derivativeY[0] = (unitx.cross(OM)).dot(projY);
  glbl_derivativeY[1] = (unity.cross(OM)).dot(projY);
  glbl_derivativeY[2] = (unitz.cross(OM)).dot(projY);

  /*
  // note: the global derivative sign must be reversed from the ATLAS paper 
  // because mille wants the derivative of the fit, not of the residual.
  for(unsigned int i = 0; i < 6; ++i)
    {
      glbl_derivativeX[i] *= -1.0; 
      glbl_derivativeY[i] *= -1.0; 
    }
  */
  if(Verbosity() > 1)
    {
      std::cout << " glbl_derivativesX for layer " << layer << std::endl;
      for(unsigned int i = 0; i < 6; ++i)
	{
	  std::cout << " i " << i << " glbl_derivative " << glbl_derivativeX[i] << std::endl;
	}
      
      std::cout << " glbl_derivativesY for layer " << layer << std::endl;
      for(unsigned int i = 0; i < 6; ++i)
	{
	  std::cout << " i " << i << " glbl_derivative " << glbl_derivativeY[i] << std::endl;
	}
    }


}
void HelicalFitter::get_projectionXY(Surface surf, std::pair<Acts::Vector3, Acts::Vector3> tangent, Acts::Vector3& projX, Acts::Vector3& projY)
{
  // we only need the direction part of the tangent
  Acts::Vector3 tanvec = tangent.second;

  // get the plane of the surface
  Acts::Vector3 sensorCenter      = surf->center(_tGeometry->geometry().getGeoContext()) / Acts::UnitConstants::cm;  // convert to cm
  // sensorNormal is the Z vector
  Acts::Vector3 Z = -surf->normal(_tGeometry->geometry().getGeoContext()) / Acts::UnitConstants::cm; 

  // get surface X and Y unit vectors in global frame
  // transform Xlocal = 1.0 to global, subtract the surface center, normalize to 1
  Acts::Vector3 xloc(1.0,0.0,0.0);
  Acts::Vector3 xglob =  surf->transform(_tGeometry->geometry().getGeoContext()) * (xloc *  Acts::UnitConstants::cm);
  xglob /=  Acts::UnitConstants::cm;
  Acts::Vector3 yloc(0.0,1.0,0.0);
  Acts::Vector3 yglob =  surf->transform(_tGeometry->geometry().getGeoContext()) * (yloc *  Acts::UnitConstants::cm);
  yglob /=  Acts::UnitConstants::cm;

  Acts::Vector3 X = (xglob-sensorCenter) / (xglob-sensorCenter).norm();
  Acts::Vector3 Y = (yglob-sensorCenter) / (yglob-sensorCenter).norm();

  projX = X - (tanvec.dot(X) / tanvec.dot(Z)) * Z;
  projY = Y - (tanvec.dot(Y) / tanvec.dot(Z)) * Z;

  return;
}

unsigned int HelicalFitter::addSiliconClusters(std::vector<float>& fitpars, std::vector<Acts::Vector3>& global_vec,  std::vector<TrkrDefs::cluskey>& cluskey_vec)
{

  return TrackFitUtils::addSiliconClusters(fitpars, dca_cut, _tGeometry, _cluster_map, global_vec, cluskey_vec);
}

 bool HelicalFitter::is_layer_fixed(unsigned int layer)
 {
  bool ret = false;
  auto it = fixed_layers.find(layer);
  if(it != fixed_layers.end()) 
    ret = true;

  return ret;
 }

 void HelicalFitter::set_layer_fixed(unsigned int layer)
 {
   fixed_layers.insert(layer);
 }

 
bool HelicalFitter::is_layer_param_fixed(unsigned int layer, unsigned int param)
 {
  bool ret = false;
  std::pair<unsigned int, unsigned int> pair = std::make_pair(layer, param);
  auto it = fixed_layer_params.find(pair);
  if(it != fixed_layer_params.end()) 
    ret = true;

  return ret;
 }

void HelicalFitter::set_layer_param_fixed(unsigned int layer, unsigned int param)
 {
   std::pair<unsigned int, unsigned int> pair = std::make_pair(layer, param);
   fixed_layer_params.insert(pair);
 }

void HelicalFitter::set_tpc_sector_fixed(unsigned int region, unsigned int sector, unsigned int side)
 {
   // make a combined subsector index
   unsigned int subsector = region * 24 + side * 12 + sector;
   fixed_sectors.insert(subsector);
 }

bool HelicalFitter::is_tpc_sector_fixed(unsigned int layer, unsigned int sector, unsigned int side)
 {
   bool ret = false;
   unsigned int region = AlignmentDefs::getTpcRegion(layer);
   unsigned int subsector = region * 24 + side * 12 + sector;
   auto it = fixed_sectors.find(subsector);
   if(it != fixed_sectors.end()) 
     ret = true;
  
   return ret;
 }

void HelicalFitter::correctTpcGlobalPositions(std::vector<Acts::Vector3> global_vec,  std::vector<TrkrDefs::cluskey> cluskey_vec)
{
  for(unsigned int iclus=0;iclus<cluskey_vec.size();++iclus)
    {
      auto cluskey = cluskey_vec[iclus];
      auto global = global_vec[iclus];
      const unsigned int trkrId = TrkrDefs::getTrkrId(cluskey);	  
      if(trkrId == TrkrDefs::tpcId) 
	{  
	  // have to add corrections for TPC clusters after transformation to global
	  int crossing = 0;  // for now
	  makeTpcGlobalCorrections(cluskey, crossing, global); 
	  global_vec[iclus] = global;
	}
    }
}

