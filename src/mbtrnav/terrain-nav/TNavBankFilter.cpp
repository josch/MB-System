/* FILENAME      : TNavBankFilter.cpp
 * AUTHOR        : Debbie Meduna and Peter Kimball
 * DATE          : 05/06/09
 *
 * LAST MODIFIED : 07/15/10
 * MODIFIED BY   : Debbie Meduna
 * -----------------------------------------------------------------------------
 * Modification History
 * -----------------------------------------------------------------------------
 ******************************************************************************/

#include "TNavConfig.h"
#include "TNavBankFilter.h"
#include "mapio.h"
#include "TNavPFLog.h"

#define MAX_CROSS_BEAM_COMPARISONS  5

double
WeightArray::
operator[](int index){
    if(index < 0 || index >= this->size){
        return 0;
    }
    else{
        return weights[index];
    }
}

//TNavBankFilter:: //Reload Map Issue
//TNavBankFilter(char* mapName, char* vehicleSpecs, char* directory, const double* windowVar,
//				   const int& mapType) : TNavFilter(mapName, vehicleSpecs, directory, windowVar, mapType) {
TNavBankFilter::
TNavBankFilter(TerrainMap* terrainMap, char* vehicleSpecs, char* directory, const double* windowVar,
               const int& mapType)
: TNavFilter(terrainMap, vehicleSpecs, directory, windowVar, mapType), numFilters(0), bfLogs(NULL), logCount(0)
{
    initVariables();
    this->tempUseBeam = new bool[TRN_MAX_BEAMS];
    this->useBeam     = new bool[TRN_MAX_BEAMS];

}



TNavBankFilter::
~TNavBankFilter() {
    if(saveDirectory != NULL) {
        //close data files
        allParticlesFile.close();
        resampParticlesFile.close();
        homerParticlesFile.close();
        homerMmseFile.close();
        measWeightsFile.close();
    }
    deleteLogs();
    delete [] tempUseBeam;
    delete [] useBeam;
}

int
TNavBankFilter::
deleteLogs()
{
    int retval=0;
    // check both array pointer and number of logs.
    if (NULL != bfLogs && logCount > 0)
    {
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::deleting logs bfLogs[%p] logCount[%u]\n", bfLogs, logCount);

        // iterate over the actual size of the array
        for (int i=0; i < logCount;  i++)
        {
            if (bfLogs[i]) delete bfLogs[i];
            bfLogs[i] = NULL;
            retval++;
        }
        logCount = 0;
        free(bfLogs);
        bfLogs = NULL;
    } else {
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::no logs to delete bfLogs[%p] logCount[%u]\n", bfLogs, logCount);
    }
    return retval;
}

int
TNavBankFilter::
allocateLogs()
{
    // remove existing logs, if any
    deleteLogs();

    bfLogs = (TNavPFLog **)malloc(numFilters * sizeof(TNavPFLog*));
    memset(bfLogs,0,numFilters * sizeof(TNavPFLog*));
    for (int i=0; i < numFilters; i++)
    {
        char lname[TNAVPF_LOGFILE_NAMELEN];
        char mname[TNAVPF_MNEM_NAMELEN];
        snprintf(lname, sizeof(lname), "%s_%d", TNavBFLogName, i+1);
        snprintf(mname, sizeof(mname), "%s_%d", TNavBFMnemName, i+1);
        bfLogs[i] = new TNavPFLog(DataLog::BinaryFormat, lname, mname);
        logCount++;
    }
    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::allocated %u logs\n", logCount);
    return logCount;
}

/* Override setModifiedWeighting() to decode the embedded parameter values
 for numFilters, PmfGridSize, and PmfGridResolution
 */
void
TNavBankFilter::
setModifiedWeighting(const int use)
{
    //default values for modified weighting, PMF grid size, and PMF grid resolution
    TNavFilter::setModifiedWeighting(TRN_FORCE_SUBCL);
    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::setModifiedWeighting umw: %d \n",
         useModifiedWeighting);


    //filter configuration set by 1s digit
    //filter comparison set by 10s digit
    //Threshold for inter-filter distance (in stdev) set by 100s digit
    //threshold for inter filter weight ratio set by 10000s and 1000s digits

    //0 means use current value for the thresholds

    int slice;
    slice = (use % 100000 / 1000);//2 decimil digits
    if(slice > 0){
        this->filterComparisonWeightRatioThreshold = (double) slice / 100.0;
    }

    slice = (use % 1000) / 100;
    if(slice > 0){
        this->filterComparisonGaussianDistanceThreshold = (double) slice;
    }

    slice = (use % 100) / 10;
    this->filterComparison = slice;

    slice = use % 10;
    this->filterConfiguration = slice;


    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
         "TNavBF::Filter configuration: %d Filter comparison: %d Distance threshold %0.1f Weight ratio threshold %0.2f\n",
         this->filterConfiguration, this->filterComparison,
         this->filterComparisonGaussianDistanceThreshold, this->filterComparisonWeightRatioThreshold);

    int currentNF;
    switch(filterConfiguration){
        case 0:
            currentNF = setNumFilters(1);
            logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::numFilters: %d \n",
                 numFilters);
            break;
        case 1:
            currentNF = setNumFilters(2);
            logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::numFilters: %d \n",
                 numFilters);
            break;
        case 2:
            currentNF = setNumFilters(3);
            logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::numFilters: %d \n",
                 numFilters);
            break;
        case 3:
            currentNF = setNumFilters(6);
            logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::numFilters: %d \n",
                 numFilters);
            break;
        case 4:
            currentNF = setNumFilters(6);
            logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::numFilters: %d \n",
                 numFilters);
            break;
        default:
            currentNF = setNumFilters(1);
            logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::defaulting to numFilters: %d \n",
                 numFilters);
            break;
    }

    // allocate logs for filters (will delete any existing logs)
    allocateLogs();
}

//********************************************************************************

void
TNavBankFilter::
initFilter(poseT& initNavPose) {
    particleT initGuess;

    //Fill in particle pose information with poseT struct
    initGuess.position[0] = initNavPose.x;
    initGuess.position[1] = initNavPose.y;
    initGuess.position[2] = initNavPose.z;
    initGuess.attitude[0] = initNavPose.phi;
    initGuess.attitude[1] = initNavPose.theta;
    initGuess.attitude[2] = initNavPose.psi;
    initGuess.compassBias = 0;
    initGuess.psiBerg     = 0;
    initGuess.gyroBias[0] = 0;
    initGuess.gyroBias[1] = 0;
    if(INTEG_PHI_THETA) {
        initGuess.gyroBias[2] = 0;
    }

    for(int i = 0; i < 3; i++) {
        initGuess.terrainState[i] = 0;
        initGuess.alignState[i] = 0;
        initGuess.dvlBias[i] = 0;
    }
    initGuess.dvlScaleFactor = 0;

    //for(int i = 0; i < 4; i++) initGuess.expectedMeasDiff[i] = 0.0;
    initGuess.expectedMeasDiff.clear();

    //initialize particle distribution using initial navigation pose
    initParticleDist(initGuess);
    for(int filterIndex = 0; filterIndex < numFilters; filterIndex++){
        for(int indexP = 0; indexP < nParticles; indexP++){
            this->depthBias[filterIndex][indexP] = 0;
        }
        this->depthBiasCov[filterIndex] = 25;
    }

    // So that terrainMap->loadSubMap can tell when to switch tiles.
    navData_x_ = initNavPose.x;
    navData_y_ = initNavPose.y;

}

//********************************************************************************

bool
TNavBankFilter::
measUpdate(measT& currMeas) {
    this->useModifiedWeighting = TRN_FORCE_SUBCL;

    Matrix beamsVF(3, currMeas.numMeas);
    Matrix tempBeamsVF(3, currMeas.numMeas);
    Matrix beamsIF(3, currMeas.numMeas);
    int beamIndices[currMeas.numMeas];  //beamsVF to currMeas index correspondence
    //	double sumSquaresWeights = 0;
    double sumMeasWeights;
    //double effSampSize = 0.0;
    int nBeamsUsed=0;
    double attitude[3] = {lastNavPose->phi, lastNavPose->theta, lastNavPose->psi};
    int mapStatus;
    //double avgWeights=0.0;
    bool successfulMeas = false;

    //double nisVal = 0.0;
    double totalVar[currMeas.numMeas];
    double mapVar = 1;//map variance for adding into sensor variance
    double modMapVar = 0.01;//map variance for calculating delta_rms and alpha

    // initialize beamIndices
    // some C versions may not permit array[n]={0} initialization
    for(int i=0;i<currMeas.numMeas;i++)beamIndices[i]=0;

    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::Projecting Measurements \n");

    //if the current measurement is a homer measurement, use homerMeasUpdate()
    if(currMeas.dataType == TRN_SENSOR_PENCIL) {
        return homerMeasUpdate(currMeas);
    }

    //if searching over DVL alignment, only project into the sensor frame
    //Both of these functions adjust beamsVF and currMeas to hold only the valid beams
    if(SEARCH_ALIGN_STATE) {
        successfulMeas = projectMeasSF(beamsVF, currMeas, beamIndices);
    } else {
        successfulMeas = projectMeasVF(beamsVF, currMeas, beamIndices);
    }

    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
         "TNavBF::Measurements Projected, beam correspondences:");

    for (int i=0; i<currMeas.numMeas; i++) {
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
             "TNavBF:: beamIndex[%i] = %i", i, beamIndices[i]);
    }
    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF:: *** beamsVF.Ncols() = %d ***", beamsVF.Ncols());
    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF:: *** currMeas.numMeas = %d ***", currMeas.numMeas);



    //if projection was successful, load relevant submap and compute
    //measurement weights for each particle
    if(successfulMeas) {

        //Load a sub-map for use in measurement correlation
        //Find the bounds of the North/East rectangle which circumscribes the
        //particle distribution
        mapStatus = defineAndLoadSubMap(beamsVF);

        // check that the map extraction worked correctly
        if(mapStatus == MAPBOUNDS_OUT_OF_BOUNDS) {
            logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBankFilter::Measurement from time = %.2f sec. not included; "
                 "unable to successfully extract a map segment for correlation", currMeas.time);
            successfulMeas = false;
        } else {

            //If we are not searching over attitude, we can perform rotation first
            if(!ALLOW_ATTITUDE_SEARCH && !SEARCH_PSI_BERG) {
                if( currMeas.dataType == TRN_SENSOR_MB )
                {
                    //
                    // Caress is providing multibeam data in the
                    // along-track/cross-track/down frame.  So we only need to do
                    // the yaw rotation to get back to NED.  This also assumes
                    // that resonSeabat8101_specs.cfg has all zero angles.
                    //
                    double tempAttitude[3] =  {0., 0., lastNavPose->psi};

                    beamsVF = applyRotation(tempAttitude, beamsVF);

                }
                else
                {
                    //
                    // Take the beams from the body frame into NED.
                    //
                    beamsVF = applyRotation(attitude, beamsVF);
                }
            }
            //
            // Only used when searching psi berg.
            tempBeamsVF = beamsVF;
            beamsIF = beamsVF;
            double tempAttitude[3];

            //Get the expected measurement differences
            for(int i=0; i < currMeas.numMeas && i < TRN_MAX_BEAMS; i++ )
            {
                this->useBeam[i]=true;
            }
            for(int i = 0; i < nParticles; i++) {
                if(!ALLOW_ATTITUDE_SEARCH && SEARCH_PSI_BERG)
                {
                    //
                    // tempBeamsVF stores beamsVF so that each particle does its own rotation.
                    tempAttitude[0] = attitude[0];
                    tempAttitude[1] = attitude[1];
                    tempAttitude[2] = attitude[2] - allParticles[i].psiBerg;

                    beamsVF = applyRotation(tempAttitude, tempBeamsVF);
                }
                //Edit to allow using only one beam from a measurement
                // sets this->tempUseBeam


                getExpectedMeasDiffParticle(allParticles[i], beamsVF, currMeas.ranges, beamIndices, mapVar);

                for( int indx=0; indx < beamsVF.Ncols(); indx++ )
                {
                    this->useBeam[indx] = this->useBeam[indx] && this->tempUseBeam[indx];
                }

                //
                // Check for this particular particle:
                nBeamsUsed = 0;
                for(int j = 0; j < beamsVF.Ncols(); j++) {
                    if(this->tempUseBeam[j])
                    {
                        nBeamsUsed++;
                    }
                }

                bool atLeastOneBeamGood = nBeamsUsed > 0;

                //if(!getExpectedMeasDiffParticle(allParticles[i], beamsVF, currMeas.ranges, beamIndices, mapVar)) {
                //if any of the measurement projections fail due to falling in NaN region of map!


                //if(!atLeastOneBeamGood && !USE_SUBCLOUD_COMPARISON){
                if(!atLeastOneBeamGood && (TRN_WT_SUBCL != this->useModifiedWeighting  && TRN_FORCE_SUBCL != this->useModifiedWeighting)){
                    //none of the beams was good for this particular particle.
                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                         "TNavBF::Measurement from time = %.2f sec. not included.",currMeas.time);
                    //"encountered NaN values in the correlation map segment for all beams on one particle.\n",
                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                         "Particle[%d] has NaN for all beam ranges, with roll = %.1f, "
                         "pitch = %.1f, yaw = %.1f degrees.\n",
                         i, allParticles[i].attitude[0]*180./PI,
                         allParticles[i].attitude[1]*180./PI,
                         allParticles[i].attitude[2]*180./PI);
                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                         "x = %.1f, y = %.1f z = %.1f.\n",
                         allParticles[i].position[0],
                         allParticles[i].position[1],
                         allParticles[i].position[2]);
                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                         "[ %.1f  %.1f  %.1f  %.3f  %.3f  %.3f];\n",
                         allParticles[i].position[0],
                         allParticles[i].position[1],
                         allParticles[i].position[2],
                         allParticles[i].attitude[0],
                         allParticles[i].attitude[1],
                         allParticles[i].attitude[2]);
                    return false;
                }
                // release resources allocated in call to
                // getExpectedMeasDiffParticle(), above
                //if( (i+1) != nParticles) delete [] useBeam;
            }

            bool test_beams = false;
            for( int indx=0; indx < beamsVF.Ncols(); indx++ )
            {
                test_beams = test_beams || this->useBeam[indx];
            }
            //
            // Should say "There is no specific good beam that all particles have in common."
            if(!test_beams) logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                                 //"At least one particle had a bad expectation for each beam.\n");
                                 "There is no specific good beam that all particles have in common.\n");

            if(test_beams && (TRN_FORCE_SUBCL == this->useModifiedWeighting)){
                for(int indexM = 0; indexM < beamsVF.Ncols(); indexM++){
                    this->useBeam[indexM] = false;
                }
                test_beams = false;
                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                     "Forcing Subcloud Comparison\n");
            }

            for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){

                bfLogs[filterIndex]->setUsedBeams(nBeamsUsed);

                sumMeasWeights = 0;
                double sumWeights = 0;
                //				sumSquaresWeights = 0;

                bool conditionForUsingBeamInFilter[TRN_MAX_BEAMS];
                for(int indexM = 0; indexM < beamsVF.Ncols(); indexM++){
                    //Beam-Filter selection goes here
                    //two stage selection:
                    //- stage 1 is beam based; if doing area selection, filters use all beams
                    //- stage 2 is area based; the filter selects the regions of the map to treat the expected measurements as if they were Nan


                    //filterConfiguration sets numFilters
                    //switch(configuration) sets conditionForUsingBeamInFilter
                    //switch(configuration) sets AreaCheckFunction
                    //- each AreaCheckFunction takes filterIndex and beamEndPoint and returns bool treatExpectedMeasurementAsNan
                    //configuration sets bool reinitOnConvergence

                    /* DVL
                     #
                     #                  x
                     #                  ^
                     #                  |
                     #                1 | 3
                     #                  |------> y
                     #                4   2
                     #                       */
                    switch(this->filterConfiguration){
                            //configurations:
                            //1 filter only
                        case 0:
                            conditionForUsingBeamInFilter[indexM] = true;
                            break;

                            //2 beam detection L/R
                        case 1:
                            //dvl test is different from reson test
                            if(currMeas.dataType == TRN_SENSOR_DVL){
                                if(beamIndices[indexM] == 0 || beamIndices[indexM] == 4){
                                    conditionForUsingBeamInFilter[indexM] = (filterIndex == 0);
                                }else{
                                    conditionForUsingBeamInFilter[indexM] = (filterIndex == 1);
                                }
                            }
                            else if(currMeas.dataType == TRN_SENSOR_MB){
                                if(beamIndices[indexM] < 6){
                                    conditionForUsingBeamInFilter[indexM] = (filterIndex == 0);
                                }else{
                                    conditionForUsingBeamInFilter[indexM] = (filterIndex == 1);
                                }
                            }
                            break;
                            //3 beam detection L/M/R
                        case 2:
                            //dvl test is different from reson test
                            if(currMeas.dataType == TRN_SENSOR_DVL){
                                if((beamIndices[indexM] == 0) || (beamIndices[indexM] == 4)){
                                    conditionForUsingBeamInFilter[indexM] = (filterIndex == 0);
                                }else{
                                    conditionForUsingBeamInFilter[indexM] = (filterIndex == 1);
                                }
                            }
                            //3 filter only with
                            else if(currMeas.dataType == TRN_SENSOR_MB){
                                if(beamIndices[indexM] < 5){
                                    conditionForUsingBeamInFilter[indexM] = (filterIndex == 0);
                                }else if(beamIndices[indexM] > 7){
                                    conditionForUsingBeamInFilter[indexM] = (filterIndex == 1);
                                }else{
                                    conditionForUsingBeamInFilter[indexM] = (filterIndex == 2);
                                }
                            }
                            break;
                            //all area based selections
                        case 3:
                        case 4:
                        case 5:
                            conditionForUsingBeamInFilter[indexM] = true;
                            break;
                        default:
                            conditionForUsingBeamInFilter[indexM] = true;
                            break;
                            //group of tiles
                            //- 3 filters
                            //- 11 filters
                            //
                            //group of along track stripes
                            //- 3 groups of 10m wide stripes
                            //- 6 groups of half overlapping 10m wide stripes
                            //
                            //3 along track stripes L/M/R
                            //across track reinit once converged -- init new filter every N meters
                            //mix of across track reinit once converged and 3 along track

                    }
                }



                for(int indexM = 0; indexM < beamsVF.Ncols(); indexM++){
                    double K = this->depthBiasCov[filterIndex] / (this->depthBiasCov[filterIndex] + mapVar + currMeas.covariance[beamIndices[indexM]]);
                    if(conditionForUsingBeamInFilter[indexM]){

                        bool loggedNan = false;
                        for(int indexP = 0; indexP < nParticles; indexP++){
                            if(this->treatBeamAsNan(filterIndex, beamsIF.SubMatrix(1,3,indexM+1,indexM+1), allParticles[indexP])){
                                continue;
                            }
                            double tmpx = (1-K) * this->depthBias[filterIndex][indexP] + K * allParticles[indexP].expectedMeasDiff[indexM];

                            if(ISNIN(tmpx)){
                                if(!loggedNan){
                                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                                         "Filter %i\tdeapthBias would have been set to Nan. indexP: %i\tindexM: %i\n", filterIndex, indexP, indexM);
                                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                                         "Old Depth Bias: %0.2f\tDepthBiasCov: %0.2f\tK: %0.3f\tExpectedMeasDiff: %0.2f\tcurrMeas.covariance: %0.2f\n",
                                         this->depthBias[filterIndex][indexP], this->depthBiasCov[filterIndex], K, allParticles[indexP].expectedMeasDiff[indexM], currMeas.covariance[beamIndices[indexM]]);
                                    loggedNan = true;
                                }
                            }else{
                                this->depthBias[filterIndex][indexP] = tmpx;
                            }

                            if(ISNIN(this->depthBias[filterIndex][indexP])){
                                this->depthBias[filterIndex][indexP] = 0.0;
                                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                                     "Filter %i\tdeapthBias set to zero to clear nan issue. indexP: %i\n", filterIndex, indexP);
                            }

                        }
                        this->depthBiasCov[filterIndex] = (1-K) * this->depthBiasCov[filterIndex];
                    }
                }





                //BEGIN SUBCLOUD COMPARISON
                //if(!test_beams && USE_SUBCLOUD_COMPARISON){
                if((!test_beams && TRN_WT_SUBCL == this->useModifiedWeighting ) || (TRN_FORCE_SUBCL == this->useModifiedWeighting)){
                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                         "\nFilter %i\tWeighting particles with subcloud comparison\n", filterIndex);
                    bool atLeastOneBeamUsed = false;

                    //tempWeights allows reverting (ignoring this measirement) if it results in Nan values somehow
                    double tempWeights[nParticles];
                    double tempWindowedNis[nParticles];
                    int numBeamsForEachParticle[nParticles];
                    for(int indexP = 0; indexP < nParticles; indexP++) {
                        tempWeights[indexP] = this->weights[filterIndex].weights[indexP];
                        tempWindowedNis[indexP] = 0.0;
                        numBeamsForEachParticle[indexP] = 0;
                    }

                    //loop through beams; find subcloud for each beam; adjust subcloud weights
                    for(int indexM = 0; indexM < beamsVF.Ncols(); indexM++){
                        if(useBeam[indexM] || !conditionForUsingBeamInFilter[indexM]){
                            continue;
                        }

                        //particle indices in subcloud
                        int particleIndicies[nParticles];
                        int numParticlesWithBeamM = 0;

                        //indices for particles not in the subcloud; needed for correctly handling their weights
                        int nonSubcloudIndicies[nParticles];
                        int nonSubcloudCount = 0;

                        //we need to normalize the conditional distribution of particles with beam M
                        double tempSubcloudWeights[nParticles];
                        double sumWeightsInSubcloud = 0.0;

                        for(int indexP = 0; indexP < nParticles; indexP++){
                            if(!ISNIN(allParticles[indexP].expectedMeasDiff[indexM]) && !this->treatBeamAsNan(filterIndex, beamsIF.SubMatrix(1,3,indexM+1,indexM+1), allParticles[indexP])){
                                particleIndicies[numParticlesWithBeamM] = indexP;
                                tempSubcloudWeights[numParticlesWithBeamM] = this->weights[filterIndex].weights[indexP];
                                sumWeightsInSubcloud += tempSubcloudWeights[numParticlesWithBeamM];
                                numParticlesWithBeamM++;
                                numBeamsForEachParticle[indexP]++;
                            }
                            else{
                                nonSubcloudIndicies[nonSubcloudCount] = indexP;
                                nonSubcloudCount++;
                            }
                        }
                        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                             "Filter %i\tbeam number: %i\tnum in subcloud: %i\tnum not in subcloud: %i\n", filterIndex, indexM, numParticlesWithBeamM, nonSubcloudCount);

                        bfLogs[filterIndex]->setSubcloudCounts(indexM, numParticlesWithBeamM);

                        if((numParticlesWithBeamM < 0.001 * nParticles) || (sumWeightsInSubcloud < 0.001)){
                            logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                                 "Filter %i\tinsufficient particles or particle weight in subcloud for beam %i\n", filterIndex, indexM);
                            continue;
                        }

                        //for calculating alpha and weight updates
                        double weightUpdatesForSubcloud[nParticles];
                        double totalVariance = mapVar + currMeas.covariance[beamIndices[indexM]] + this->depthBiasCov[filterIndex];
                        double meanExpectedMeasurementDifference = 0;
                        double partialDeltaRmsComputation = 0;
                        double partialOneMinusSumSquareWeights = 1;
                        double subcloudInnovationVariance = 0.0;

                        for(int indexS = 0; indexS < numParticlesWithBeamM; indexS++){
                            double adjustedInnovation = allParticles[particleIndicies[indexS]].expectedMeasDiff[indexM] - this->depthBias[filterIndex][particleIndicies[indexS]];//averageInnovation[particleIndicies[indexS]];

                            //weight update
                            weightUpdatesForSubcloud[indexS] = exp(-0.5 * pow(adjustedInnovation,2) / totalVariance);

                            //normalize subcloud weights
                            tempSubcloudWeights[indexS] = tempSubcloudWeights[indexS]/sumWeightsInSubcloud;

                            //delta_rms_squared calculations
                            meanExpectedMeasurementDifference += adjustedInnovation * tempSubcloudWeights[indexS];
                            partialDeltaRmsComputation += adjustedInnovation * adjustedInnovation * tempSubcloudWeights[indexS];
                            partialOneMinusSumSquareWeights -= tempSubcloudWeights[indexS] * tempSubcloudWeights[indexS];

                            if(ISNIN(meanExpectedMeasurementDifference)){
                                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                                     "Filter %i\tSubcloudIndex: %i\ttempSubcloudWeight: %f\tadjustedInnovaiton: %f\texpectedMeasDiff: %f\tdepthBias: %f\tdepthCov: %f\n",
                                     filterIndex, indexS, tempSubcloudWeights[indexS], adjustedInnovation, allParticles[particleIndicies[indexS]].expectedMeasDiff[indexM],
                                     this->depthBias[filterIndex][particleIndicies[indexS]], this->depthBiasCov[filterIndex]);
                                break;
                            }

                            //for particle NIS calculations; part of Subcloud NIS
                            subcloudInnovationVariance += pow(adjustedInnovation, 2) * this->weights[filterIndex].weights[particleIndicies[indexS]] -
                            pow(adjustedInnovation * this->weights[filterIndex].weights[particleIndicies[indexS]], 2);

                            tempWindowedNis[particleIndicies[indexS]] += pow(adjustedInnovation,2) / (totalVariance + subcloudInnovationVariance);

                            /*
                             if(indexS == 0){
                             printf("averageInnovation: %f\tinnovation: %f\tadjustedInnovaiton%f\n", averageInnovation[particleIndicies[indexS]],
                             allParticles[particleIndicies[indexS]].expectedMeasDiff[indexM], adjustedInnovation);
                             }
                             */
                        }

                        double alpha;
                        double delta_rms_squared = partialDeltaRmsComputation - meanExpectedMeasurementDifference * meanExpectedMeasurementDifference - (partialOneMinusSumSquareWeights * modMapVar);
                        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                             "Filter %i\tpartialDeltaRmsComputation: %f\tterrainVariance: %f\t1-SumSquareWeights: %f\n", filterIndex, partialDeltaRmsComputation, partialDeltaRmsComputation - meanExpectedMeasurementDifference * meanExpectedMeasurementDifference, partialOneMinusSumSquareWeights);

                        if(delta_rms_squared <= 0){
                            alpha = 0;
                        }
                        else{
                            alpha = (delta_rms_squared *(mapVar + currMeas.covariance[beamIndices[indexM]]))
                            / ((delta_rms_squared + modMapVar) * (mapVar + currMeas.covariance[beamIndices[indexM]]) + (modMapVar * (currMeas.covariance[beamIndices[indexM]] + mapVar)));
                        }
                        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                             "Filter %i\tmeanExpectedMeasDiff: %f\tdelta_rms_squared: %f\talpha: %f\n", filterIndex, meanExpectedMeasurementDifference, delta_rms_squared, alpha);

                        bfLogs[filterIndex]->setMeanExpMeasDif(indexM, meanExpectedMeasurementDifference);
                        bfLogs[filterIndex]->setAlpha(indexM, alpha);

                        //apply alpha
                        for(int indexS = 0; indexS < numParticlesWithBeamM; indexS++){
                            weightUpdatesForSubcloud[indexS] = pow(weightUpdatesForSubcloud[indexS], alpha);
                        }

                        //calculate eta (normalization constant for subcloud weights)
                        double etaNumerator = 0;
                        double etaDenominator = 0;
                        for(int indexS = 0; indexS < numParticlesWithBeamM; indexS++){
                            etaDenominator += this->weights[filterIndex].weights[particleIndicies[indexS]] * weightUpdatesForSubcloud[indexS];
                            etaNumerator += this->weights[filterIndex].weights[particleIndicies[indexS]];
                        }

                        //apply weight updates to tempWeights in subcloud
                        for(int indexS = 0; indexS < numParticlesWithBeamM; indexS++){
                            tempWeights[particleIndicies[indexS]] *= weightUpdatesForSubcloud[indexS];
                        }

                        //apply weight updates to tempWeights not in subcloud
                        double oneOverEta =  etaDenominator / etaNumerator;
                        for(int indexS = 0; indexS < nonSubcloudCount; indexS++){
                            tempWeights[nonSubcloudIndicies[indexS]] *= oneOverEta;
                        }
                        atLeastOneBeamUsed = true;
                    }
                    //particle windowed NIS update
                    this->SubcloudNIS = 0;
                    for(int indexP = 0; indexP < nParticles; indexP++){
                        if(numBeamsForEachParticle[indexP] > 0){
                            //old allParticles[indexP].windowedNis[allParticles[indexP].windowIndex] = tempWindowedNis[indexP] / numBeamsForEachParticle[indexP];
                            this->windowedNis[filterIndex][indexP][this->windowIndex[filterIndex][indexP]] = tempWindowedNis[indexP] / numBeamsForEachParticle[indexP];
                            //old allParticles[indexP].windowIndex = (allParticles[indexP].windowIndex + 1) % 20;
                            this->windowIndex[filterIndex][indexP] = (this->windowIndex[filterIndex][indexP] + 1) % 20;
                        }

                        double particleNisValue = 0;
                        for(int indexW = 0; indexW < 20; indexW ++){
                            //old particleNisValue += allParticles[indexP].windowedNis[indexW];
                            particleNisValue += this->windowedNis[filterIndex][indexP][indexW];

                        }

                        this->SubcloudNIS += this->weights[filterIndex].weights[indexP] * particleNisValue/20.0;

                    }
                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                         "Filter %i\tSubcloudNIS: %f\n", filterIndex, this->SubcloudNIS);

                    bfLogs[filterIndex]->setSubcloudNIS(this->SubcloudNIS);


                    //check for nan values before allowing the update into the filter weights
                    bool nanWeights = false;
                    double tempSumWeights = 0;
                    for(int indexP = 0; indexP < nParticles; indexP++) {
                        nanWeights = nanWeights || ISNIN(tempWeights[indexP]);
                        tempSumWeights += tempWeights[indexP];
                    }
                    if(nanWeights){
                        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                             "Filter %i\tSubcloud weighting FAILED due to NAN weights.\n", filterIndex);
                    } else if(tempSumWeights == 0){
                        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                             "Filter %i\tSubcloud Weighting FAILED due to sumWeights == 0. \n", filterIndex);
                    } else if(!atLeastOneBeamUsed){
                        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                             "Filter %i\tNo beams used in subcloud update\n", filterIndex);
                    } else{
                        for(int indexP = 0; indexP < nParticles; indexP++) {
                            this->weights[filterIndex].weights[indexP] = tempWeights[indexP];
                        }
                    }

                }
                //END SUBCLOUD COMPARISON


                /* BEGIN cross beam comparison edits*/
                // Only used when there are no beams with good expectations on all particles

                //int MAX_CROSS_BEAM_COMPARISONS = 5;
                //int USE_CROSS_BEAM_COMPARISON = 1;

                /*Force Cross beam Comparison even when normal weighting can be done
                 for(int indexM=0; indexM < beamsVF.Ncols(); indexM++){
                 useBeam[indexM] = false;
                 }
                 temp = false;
                 */

                // used to be if((!temp && USE_CROSS_BEAM_COMPARISON) && !(SEARCH_ALIGN_STATE || ALLOW_ATTITUDE_SEARCH || SEARCH_PSI_BERG)){
                if((!test_beams && TRN_WT_XBEAM == this->useModifiedWeighting) &&
                   !(SEARCH_ALIGN_STATE || ALLOW_ATTITUDE_SEARCH || SEARCH_PSI_BERG)){
                    // !temp means no beams are good for normal comparison
                    //the SEARCH_* flags would have each particle have a different orientation relative to the map,
                    //	which isn't accounted for yet.
                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                         "Filter %i\tWeighting particles with cross beam comparison.\n", filterIndex);

                    //compile a list of beams to use for cross beam comparison
                    //	can be different beams for each particle (that's the point)
                    //	max of MAX_CROSS_BEAM_COMPARISONS
                    //	Also find the particle with the fewest good beams in case it's less
                    //	currently takes beams in numbered order rather than randomly or ordered by terrain information
                    int numGoodBeamsParticle[nParticles];
                    int goodBeamIndicies[nParticles * MAX_CROSS_BEAM_COMPARISONS];
                    for(int indexP = 0; indexP < nParticles; indexP++) {
                        numGoodBeamsParticle[indexP] = 0;
                    }

                    int minNumBeams = beamsVF.Ncols();
                    for(int indexP = 0; indexP < nParticles; indexP++) {
                        for(int indexM=0; indexM < beamsVF.Ncols(); indexM++){

                            // No nan beams, and no beams which can be used normally
                            // if(!(isnan(allParticles[indexP].expectedMeasDiff[indexM]) || useBeam[indexM])){
                            if(!(ISNIN(allParticles[indexP].expectedMeasDiff[indexM]) || useBeam[indexM] || !conditionForUsingBeamInFilter[indexM])){
                                goodBeamIndicies[indexP * MAX_CROSS_BEAM_COMPARISONS + numGoodBeamsParticle[indexP]] = indexM;

                                numGoodBeamsParticle[indexP] += 1;
                                if(numGoodBeamsParticle[indexP] >=MAX_CROSS_BEAM_COMPARISONS){
                                    break;
                                }
                            }
                        }
                        if(minNumBeams > numGoodBeamsParticle[indexP]){
                            minNumBeams = numGoodBeamsParticle[indexP];
                        }
                    }

                    //tempWeights allows reverting (ignoring this measirement) if it results in Nan values somehow
                    double tempWeights[nParticles];
                    for(int indexP = 0; indexP < nParticles; indexP++) {
                        tempWeights[indexP] = this->weights[filterIndex].weights[indexP];
                    }

                    //compute the weight updates
                    double tempWeightUpdate[nParticles];
                    for(int beamNumber=0; beamNumber < minNumBeams; beamNumber++){

                        double partialDeltaRmsComputation = 0;
                        double partialMeanTerrainDepth = 0;
                        double partialOneMinusSumSquareWeights = 1;
                        double maxSensorVar = 0;

                        //for each particle
                        //	pick a beam
                        //	calculate it's likelihood with p(h)==1 assumption
                        //	calculate it's contribution to delta_rms (alpha precursor)
                        for(int indexP = 0; indexP < nParticles; indexP++) {

                            double totalVariance = 10;
                            totalVariance = mapVar + currMeas.covariance[beamIndices[goodBeamIndicies[indexP * MAX_CROSS_BEAM_COMPARISONS + beamNumber]]];
                            if(maxSensorVar < currMeas.covariance[beamIndices[goodBeamIndicies[indexP * MAX_CROSS_BEAM_COMPARISONS + beamNumber]]]){
                                maxSensorVar = currMeas.covariance[beamIndices[goodBeamIndicies[indexP * MAX_CROSS_BEAM_COMPARISONS + beamNumber]]];
                            }

                            tempWeightUpdate[indexP] = exp(-0.5 * pow(allParticles[indexP].expectedMeasDiff[goodBeamIndicies[indexP * MAX_CROSS_BEAM_COMPARISONS + beamNumber]] -
                                                                      this->depthBias[filterIndex][indexP],2) / totalVariance);
                            double beamEndpointTerrainDepth = 0;
                            beamEndpointTerrainDepth = allParticles[indexP].position[2] + beamsVF(3, goodBeamIndicies[indexP * MAX_CROSS_BEAM_COMPARISONS + beamNumber] + 1);

                            partialDeltaRmsComputation += beamEndpointTerrainDepth * beamEndpointTerrainDepth * this->weights[filterIndex].weights[indexP];
                            partialMeanTerrainDepth += beamEndpointTerrainDepth * this->weights[filterIndex].weights[indexP];
                            partialOneMinusSumSquareWeights -= this->weights[filterIndex].weights[indexP] * this->weights[filterIndex].weights[indexP];

                        }

                        //calculate delta_rms then alpha
                        double alpha;
                        double delta_rms_squared = partialDeltaRmsComputation - partialMeanTerrainDepth * partialMeanTerrainDepth - (partialOneMinusSumSquareWeights * mapVar);
                        if(delta_rms_squared <= 0){
                            alpha = 0;
                        }
                        else{
                            //This alpha is always smaller than Shandor's for the same delta_rms (trust the measurement less)
                            alpha = delta_rms_squared / (delta_rms_squared + mapVar + maxSensorVar);
                        }

                        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                             "Filter %i\talpha: %f\tMeanTerrainDepth: %f\n", filterIndex, alpha, partialMeanTerrainDepth);
                        //set alpha for currMeas for logging
                        //!currMeas.alphas[i] = alpha; // This seems to break things when turned on


                        //modify the weight updates by alpha and apply them to the tempWeights
                        for(int indexP = 0; indexP < nParticles; indexP++) {
                            tempWeights[indexP] *= pow(tempWeightUpdate[indexP], alpha);
                        }
                    }

                    //check for nan values before allowing the update into the filter weights
                    bool nanWeights = false;
                    for(int indexP = 0; indexP < nParticles; indexP++) {
                        //nanWeights = nanWeights || isnan(tempWeights[indexP]);
                        nanWeights = nanWeights || ISNIN(tempWeights[indexP]);
                    }
                    if(nanWeights){
                        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
                             "Filter %i\tCross beam comparison FAILED due to NAN weights.\n", filterIndex);
                    }
                    else{
                        for(int indexP = 0; indexP < nParticles; indexP++) {
                            this->weights[filterIndex].weights[indexP] = tempWeights[indexP];
                        }
                    }
                }

                /* END cross beam comparison edits*/

                //Here we trigger between using the modified weighting scheme concocted by Shandor
                //and the standard TRN weighting

                //TODO: Figure out if we are going to do any different correlation for octree vs dem

                //Compute the variance used to update the particle weights using normal or modified weighting
                // used to be if(!this->useModifiedWeighting) {
                if(TRN_WT_NONE == this->useModifiedWeighting) {
                    //set variance for each beam
                    for(int i = 0; i < beamsVF.Ncols(); i++) {
                        //Need to set the map variance some how, right now will assume it is a fixed value...
                        totalVar[i] = mapVar + currMeas.covariance[beamIndices[i]];
                        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBankFilter::Filter %i\tVariance for beam %i is %.2f \n", filterIndex, beamIndices[i-1], totalVar[i]);
                    }
                }
                else
                {
                    //Implement modified algorithm
                    //*** TODO Un-hard code the number of measurements used

                    double* mapInfoCov = new double[beamsVF.Ncols()](); 	//;[4] ={0.0, 0.0, 0.0, 0.0};
                    double* mapSquared = new double[beamsVF.Ncols()](); 	//[4]  = {0.0, 0.0, 0.0, 0.0};   //
                    double* mapMean = new double[beamsVF.Ncols()](); 			//[4]  = {0.0, 0.0, 0.0, 0.0};		//mean value of expected measurement
                    double* mapVariance = new double[beamsVF.Ncols()](); 	//[4] = {0.0, 0.0, 0.0, 0.0};	//variance of expected measurements
                    double* beamVar = new double[beamsVF.Ncols()](); 			//[4] = {0.0, 0.0, 0.0, 0.0};		//variance of range measurements

                    //			double mapInfoCov[4] ={0.0, 0.0, 0.0, 0.0};
                    //			double mapSquared[4]  = {0.0, 0.0, 0.0, 0.0};   //
                    //			double mapMean[4]  = {0.0, 0.0, 0.0, 0.0};		//mean value of expected measurement
                    //			double mapVariance[4] = {0.0, 0.0, 0.0, 0.0};	//variance of expected measurements
                    //			double beamVar[4] = {0.0, 0.0, 0.0, 0.0};		//variance of range measurements

                    //Compute mean and square of expected measurement
                    for(int beamInd = 0; beamInd < beamsVF.Ncols(); beamInd++) {
                        // what if false for every iteration of the loop?
                        if(this->useBeam[beamInd] && conditionForUsingBeamInFilter[beamInd]){	//edit to allow using any good beams from measurement
                            for(int i = 0; i < nParticles; i++) {
                                //As we already have the expected measurement difference, compute mean and square of measurement difference
                                mapSquared[beamInd] += pow(allParticles[i].expectedMeasDiff[beamInd] - this->depthBias[filterIndex][i], 2) * this->weights[filterIndex].weights[i];
                                mapMean[beamInd] += (allParticles[i].expectedMeasDiff[beamInd] - this->depthBias[filterIndex][i]) * this->weights[filterIndex].weights[i];
                            }
                        }
                    }

                    double baseSensorVar = 0;

                    modMapVar = .01;  //assume map noise is about .15m^2 - but still use the value pulled from the map file
                    baseSensorVar = mapVar - modMapVar;
                    if(baseSensorVar < 0) {
                        baseSensorVar = 0;
                    }

                    //Compute variance of expected measurements and variance used in modified measurement update
                    for(int i = 0; i < beamsVF.Ncols(); i++) {

                        beamVar[i] = currMeas.covariance[beamIndices[i]];
                        mapVariance[i] = mapSquared[i] - pow(mapMean[i], 2);
                        if(mapVariance[i] > modMapVar) {
                            mapInfoCov[i] = mapVariance[i] - modMapVar;
                        } else {
                            mapInfoCov[i] = 0.0000001;
                        }

                        totalVar[i] = ((beamVar[i] + baseSensorVar + modMapVar) * mapVariance[i] + (baseSensorVar + beamVar[i]) * modMapVar) /
                        mapInfoCov[i];
                        //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBankFilter::Modified Variance for beam %i is %.2f \n", i, totalVar[i]);

                        //ALPHA
                        // Valid values are 0 <= alpha <= 1
                        // Use -0.1 as an encoding for NaN
                        //
                        if(totalVar[i] > 0.0){
                            //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Alpha[%u]\t%f\n", i, currMeas.alphas[i]);
                            currMeas.alphas[i] = (baseSensorVar + beamVar[i] + modMapVar) / totalVar[i];
                            //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Alpha[%u]\t%f\n", i, currMeas.alphas[i]);

                        }else{
                            // NaN is encoded as a value < 0
                            //
                            currMeas.alphas[i] = -0.1;

                        }
                        //END ALPHA
                    }

                    //			for (i = 0; i < beamsVF.Ncols(); i++) logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::Calculated mapVariance for beam %i as %.2f \n", i, mapVariance(i));
                    //			logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBankFilter::mapVariance = {%.2f,%.2f,%.2f,%.2f}\n",mapVariance[0],
                    //				mapVariance[1],mapVariance[2],mapVariance[3]);

                    delete [] mapInfoCov;
                    delete [] mapSquared;
                    delete [] mapMean;
                    delete [] mapVariance;
                    delete [] beamVar;
                }

                //Loop through & compute measurement update weights for all particles
                double sumSquaredError = 0;

                //		TODO: Beam Variance can be computed ahead of time (implement later)
                //		for (int beamInd = 0; beamInd < beamsVF.Ncols(); beamInd++) sumInvVar += (1.0/(totalVar[beamInd]));

                for(int i = 0; i < nParticles; i++) {
                    sumSquaredError = 0;
                    double sumWeightedError = 0;
                    double sumInvVar = 0;

                    currMeasWeights[i] = 1;

                    for(int beamInd = 0; beamInd < beamsVF.Ncols(); beamInd++) {
                        if(this->useBeam[beamInd] && conditionForUsingBeamInFilter[beamInd]){	//edit to allow using any good beams from measurement

                            //As we already have the expected measurement difference, just apply the measurement model to it
                            sumWeightedError += (1.0 / (totalVar[beamInd])) * allParticles[i].expectedMeasDiff[beamInd] - this->depthBias[filterIndex][i]; //Weighted mean error
                            sumSquaredError += (1.0 / (totalVar[beamInd])) * pow(allParticles[i].expectedMeasDiff[beamInd] - this->depthBias[filterIndex][i], 2); //Weighted Squared Error
                            sumInvVar += (1.0 / (totalVar[beamInd]));		//Beam Variance
                            //					logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF:totalVar[%i] is %f\n",beamInd,totalVar[beamInd]);
                            if(ISNIN(sumSquaredError))
                            {
                                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Filter %i\tTNavBF:Sum of squared error for particle %i beam %i is nan \n", filterIndex, i, beamInd);

                                bfLogs[filterIndex]->write();

                                return false;
                                //		     logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF:totalVar[%i] is %f\n",beamInd,totalVar[beamInd]);
                                //		     logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF:expectedMeasDiff[%i] is %f \n",beamInd,allParticles[i].expectedMeasDiff[beamInd]);
                            }
                        }
                    }

                    //Compute new measurement weight
                    if(USE_CONTOUR_MATCHING && !USE_RANGE_CORR) {
                        double currDepthBias = (1.0 / sumInvVar) * sumWeightedError;
                        allParticles[i].position[2] -= currDepthBias;
                        for(int beamInd = 0; beamInd < beamsVF.Ncols(); beamInd++) {
                            if(this->useBeam[beamInd] && conditionForUsingBeamInFilter[beamInd]){	//edit to allow using any good beams from measurement
                                allParticles[i].expectedMeasDiff[beamInd] -= currDepthBias;
                            }
                        }

                        //calculate likelihood equation.
                        //currMeasWeights[i] = exp(-0.5*(sumSquaredError-2*currDepthBias*sumWeightedError+pow(currDepthBias,2)*sumInvVar));
                        currMeasWeights[i] = exp(-0.5 * (sumSquaredError - currDepthBias * sumWeightedError));
                        //newWeight *= exp(-0.5*(currDepthBias*currDepthBias));
                    } else {
                        currMeasWeights[i] = exp(-0.5 * sumSquaredError);
                    }

                    sumWeights += this->weights[filterIndex].weights[i] * currMeasWeights[i];
                    sumMeasWeights += currMeasWeights[i];
                }

                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF:: Filter %i\tsumSquaredError = %f \n", filterIndex, sumSquaredError);
                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF:: Filter %i\tsumWeights = %f \n", filterIndex, sumWeights);

                bfLogs[filterIndex]->setSumWeights(sumWeights);
                bfLogs[filterIndex]->setSumSquaredError(sumSquaredError);

                //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::Filter %i\tCalculating NIS Matrices \n");

                //SymmetricMatrix mapMeasVarMat(beamsVF.Ncols());  			//Variance in expected map measurements
                //ColumnVector measDiffMean(beamsVF.Ncols());						//Mean difference between actual and expected measurements
                //computeInnovationsMatrices(allParticles, mapMeasVarMat, measDiffMean);  //Compute variance matrix for expected measurements

                //		logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::Current number of measurements is: %i \n",currMeas.numMeas);
                //		logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::Size of measurement matrix is: %i \n",beamsVF.Ncols());
                //		logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::Size of covariance matrix is: %i \n",mapMeasVarMat.Ncols());
                //	 for(i = 1; i < beamsVF.Ncols() + 1; i++) {
                //	    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::Calculated measDiffMean for beam %i as %.2f. Range = %.2f \n",
                //		 beamIndices[i-1], measDiffMean(i), currMeas.ranges[i-1]);
                //	 }

                //if(mapMeasVarMat.Nrows() > 0) {
                //	logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::Filter %i\tFirst Term of Map Covariance Matrix is %.2f \n", filterIndex, mapMeasVarMat(1, 1));
                //}

                //calculateNIS(mapMeasVarMat, measDiffMean, nisVal, currMeas, beamIndices);

                //old logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::Calculated NIS Value : %.2f \tnumBeams normalized NIS: %.2f\n", nisVal,nisVal/beamsVF.Ncols());
                //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::Calculated NIS Value : %.2f \tnumBeams normalized NIS: %.2f\n", nisVal*beamsVF.Ncols(),nisVal);

                //updateNISwindow(nisVal);

                //Keep track of the number of soundings used since the last resampling
                nSoundings += beamsVF.Ncols();

                bfLogs[filterIndex]->setSoundings(nSoundings);

                measVariance = 0;

                //Apply measurement weights and normalize the distribution
                if(sumWeights == 0.0){

                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"\nFilter %i\tParticle Weights not updated due to sumWeights == 0.0\n\n", filterIndex);
                }
                /*else if(nisVal>=NIS_WINDOW_LENGTH*1.4){ //TODO: Don't hard code in 1.4, use MAX_NIS_VALUE, but that #define is not defined in the scope of the particle filter
                 logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"\nParticle Weights not updated because current NIS >= %f\n",NIS_WINDOW_LENGTH*1.4);
                 }*/
                else{
                    for(int i = 0; i < nParticles; i++) {
                        this->weights[filterIndex].weights[i] *= currMeasWeights[i] / sumWeights;
                        //TODO if inovations are too large, particle weights go nan.
                        //currMeasWeight was not nan for the particular failure I examined.
                        //sumWeights == 0.0

                        //						sumSquaresWeights += pow(this->weights[filterIndex].weights[i], 2);
                        //compute variance of measurement weights
                        currMeasWeights[i] /= sumMeasWeights;
                        measVariance += pow(currMeasWeights[i] - 1.0 / nParticles, 2) / nParticles;
                        if(saveDirectory != NULL) {
                            //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"%.2f",currMeasWeights[i]);
                            measWeightsFile << currMeasWeights[i] << "\t";
                        }
                    }
                }
                //effSampSize = 1.0 / sumSquaresWeights;

                bfLogs[filterIndex]->write();

            }
            if(saveDirectory != NULL) {
                measWeightsFile << endl;
            }

        }

    }

#ifdef USE_MATLAB
    plotMapMatlab(mapForPloting.depths, mapForPloting.xpts,
                  mapForPloting.ypts, "title('Sub-Map and Post-Resampling Particle Distribution');", "figure(1)");
    mapPlotted = 1;
    plotParticleDistMatlab(allParticles, "figure(1)");
#endif

    //if measurement successfully added, recheck estimator convergence
    //if(successfulMeas)
    //   checkConvergence();


    poseT biasPose;
    for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
        this->computeMMSE(&biasPose, filterIndex);
        biasPose -= *lastNavPose;

        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),
             "Filter %i\tNorthBias: %0.1f\tEastBias: %0.1f\tDepthBias: %0.1f\tNorthVariance: %0.1f\tEastVariance: %0.1f\tDepthVariance: %0.1f\tthis->DepthBiasCov: %0.1f\n",
             filterIndex, biasPose.x, biasPose.y, biasPose.z, biasPose.covariance[0], biasPose.covariance[2], biasPose.covariance[5], this->depthBiasCov[filterIndex]);
    }

    return successfulMeas;

}

//********************************************************************************

void
TNavBankFilter::
motionUpdate(poseT& currNavPose) {

#ifdef WITH_VEL_SF_SIGMA
    // TODO: unused code, use or preserve and delete
    //	double velocity_sf_sigma[3];
#endif
    poseT diffPose;
    //double gyroStddev;

    //if no valid gps data, extract relevant velocity measurement for
    //dead-reckoning
#ifdef WITH_VEL_SF_SIGMA
    // TODO: unused code, use or preserve and delete
    //	if(!currNavPose.gpsValid && DEAD_RECKON) {
    //		velocity_sf_sigma[0] = 0.0;
    //		velocity_sf_sigma[1] = 0.0;
    //		velocity_sf_sigma[2] = 0.0;
    //
    //		//Extract PREVIOUS sensor frame velocity plus gaussian noise based on
    //		//bottom lock:
    //		if(lastNavPose->bottomLock) {
    //			velocity_sf_sigma[0] += fabs(VEL_PER_ERROR * lastNavPose->vx / 100.0);
    //			velocity_sf_sigma[1] += fabs(VEL_PER_ERROR * lastNavPose->vy / 100.0);
    //			velocity_sf_sigma[2] += fabs(VEL_PER_ERROR * lastNavPose->vz / 100.0);
    //		} else {
    //			velocity_sf_sigma[0] += fabs(WATER_VEL_PER_ERROR * lastNavPose->vx / 100.0);
    //			velocity_sf_sigma[1] += fabs(WATER_VEL_PER_ERROR * lastNavPose->vy / 100.0);
    //			velocity_sf_sigma[2] += fabs(WATER_VEL_PER_ERROR * lastNavPose->vz / 100.0);
    //
    //		}
    //
    //		//check if velocity data is old and add noise according to a
    //		//0.01m/s^2 acceleration
    //		if(this->timeLastDvlValid != lastNavPose->time) {
    //			velocity_sf_sigma[0] += 0.01 * fabs(lastNavPose->time - this->timeLastDvlValid);
    //			velocity_sf_sigma[1] += 0.01 * fabs(lastNavPose->time - this->timeLastDvlValid);
    //			velocity_sf_sigma[2] += 0.01 * fabs(lastNavPose->time - this->timeLastDvlValid);
    //		}
    //
    //		//add on constant velocity sigma
    //		velocity_sf_sigma[0] += VEL_STDDEV;
    //		velocity_sf_sigma[1] += VEL_STDDEV;
    //		velocity_sf_sigma[2] += VEL_STDDEV;
    //	}
#endif


    //Compute delta pose of the vehicle since the last update
    diffPose = currNavPose;
    diffPose -= *lastNavPose;

    double cep;
    double driftStddev;
    double fractionProbMassAdjacent, fractionProbMassCorner, fractionProbMassRemaining;
    double sumWeights;

    //Update each particle's position individually
    cep = (this->vehicle->driftRate / 100.0) * (sqrt(diffPose.x * diffPose.x + diffPose.y * diffPose.y));

    //TODO:  check conversion from CEP to Stddev, I don't feel like there should be a sqrt outside the CEP
    driftStddev = MOTION_NOISE_MULTIPLIER * sqrt(cep / sqrt(-2 * (log(1 - 0.5))));

    for(int filterIndex = 0; filterIndex < numFilters; filterIndex++){
        this->depthBiasCov[filterIndex] += driftStddev * driftStddev;
    }

    // Assumes: driftStddev << histogram resolution (2m)
    fractionProbMassAdjacent = driftStddev * 0.0362526;
    fractionProbMassCorner = driftStddev * 0.00155204;
    fractionProbMassRemaining = 1.0 - 4 * fractionProbMassCorner - 4 * fractionProbMassAdjacent;
    //printf("FIND THIS %f\t%f\t%f\t%f\n",fractionProbMassRemaining, fractionProbMassAdjacent, fractionProbMassCorner, driftStddev);


    for(int i = 0; i < nParticles; i++) {
        motionUpdateParticle(allParticles[i], diffPose);//, velocity_sf_sigma, gyroStddev);
    }

    for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
        WeightArray tempWeights(this->nParticles);
        sumWeights = 0;

        for(int i = 0; i < nParticles; i++) {
            tempWeights.weights[i] = this->weights[filterIndex].weights[i];
        }
        for(int i = 0; i < nParticles; i++) {
            this->weights[filterIndex].weights[i] = fractionProbMassRemaining * tempWeights[i] +
            fractionProbMassAdjacent * tempWeights[i + this->PmfGridSize] +
            fractionProbMassAdjacent * tempWeights[i - this->PmfGridSize] +
            fractionProbMassAdjacent * tempWeights[i + 1] +
            fractionProbMassAdjacent * tempWeights[i - 1] +
            fractionProbMassCorner * tempWeights[i + (this->PmfGridSize + 1)] +
            fractionProbMassCorner * tempWeights[i + (this->PmfGridSize - 1)] +
            fractionProbMassCorner * tempWeights[i - (this->PmfGridSize + 1)] +
            fractionProbMassCorner * tempWeights[i - (this->PmfGridSize - 1)];
            sumWeights += this->weights[filterIndex].weights[i];
        }
        for(int i = 0; i < nParticles; i++) {
            this->weights[filterIndex].weights[i] = this->weights[filterIndex].weights[i] / sumWeights;
        }
    }

    //Apply attitude measurement update if integrating for phi/theta states
    if(INTEG_PHI_THETA) {
        attitudeMeasUpdate(currNavPose);
    }


    //if(saveDirectory != NULL)
    //   writeParticlesToFile(allParticles, allParticlesFile);

    //Pass position to terrainMap.
    navData_x_ = currNavPose.x;
    navData_y_ = currNavPose.y;


#ifdef USE_MATLAB
    mapT mapForPloting;
    terrainMap->GetMapT(mapForPloting);
    plotMapMatlab(mapForPloting.depths,
                  mapForPloting.xpts,
                  mapForPloting.ypts,
                  "title('Sub-Map and Particle Distribution: After motion update');",
                  "figure(3)");

    plotParticleDistMatlab(allParticles, "figure(3)");
#endif

    return;
}

//********************************************************************************
//
// Maximum Likelihood, the particle with the highest weight.

void
TNavBankFilter::
computeMLE(poseT* mlePose) {
    particleT mleParticle;
    double maxWeight = 0;

    mleParticle = allParticles[0];

    //Find the particle with the highest weight
    for(int i = 0; i < nParticles; i++) {
        if(this->weights[0].weights[i] > maxWeight) {
            maxWeight = this->weights[0].weights[i];
            mleParticle = allParticles[i];
        }
    }

    //Set the mlePose to the pose of the particle with the highest weight
    getParticlePose(mleParticle, mlePose);

    return;
}

//********************************************************************************

//
// Minimum Mean Square Error, a weighted average.

void
TNavBankFilter::
computeMMSE(poseT* mmsePose) {
    //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::MMSE all\n");
    poseT mmseArray[MAX_NUM_FILTERS];

    for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
        computeMMSE(&mmseArray[filterIndex], filterIndex);
    }

    //normed Gaussian approximation fIlter distances calculations
    double gaussianFilterDistances[MAX_NUM_FILTERS][MAX_NUM_FILTERS];
    poseT distancePose;
    double normedDistance;
    SymmetricMatrix tempCov(3);
    ColumnVector tempDist(3);
    for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
        for(int secondFilterIndex = filterIndex+1; secondFilterIndex < this->numFilters; secondFilterIndex++){
            distancePose = mmseArray[filterIndex];
            distancePose -= mmseArray[secondFilterIndex];

            tempCov.Row(1) << mmseArray[filterIndex].covariance[0] + mmseArray[secondFilterIndex].covariance[0];
            tempCov.Row(2) << mmseArray[filterIndex].covariance[1] + mmseArray[secondFilterIndex].covariance[1] << mmseArray[filterIndex].covariance[2] + mmseArray[secondFilterIndex].covariance[2];
            tempCov.Row(3) << 0.0 << 0.0 << mmseArray[filterIndex].covariance[5] + mmseArray[secondFilterIndex].covariance[5];

            tempDist(1) = distancePose.x;
            tempDist(2) = distancePose.y;
            tempDist(3) = distancePose.z;

            ColumnVector temp(1);
            temp = (tempDist.t() * (tempCov.i() * tempDist));

            normedDistance = pow(temp(1), 0.5);

            gaussianFilterDistances[filterIndex][secondFilterIndex] = normedDistance;
            logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "filterIndex: %d\tindex2: %d\tnormedDistance: %0.2f\n", filterIndex, secondFilterIndex, normedDistance);
            /*logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "|%3.1f|\t|%3.1f\t%3.1f\t%3.1f|\t|%3.1f|\n", tempDist(1), tempCov(1,1), tempCov(1,2), tempCov(1,3), tempDist(1));
             logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "|%3.1f|\t|%3.1f\t%3.1f\t%3.1f|\t|%3.1f|\n", tempDist(2), tempCov(2,1), tempCov(2,2), tempCov(2,3), tempDist(2));
             logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "|%3.1f|\t|%3.1f\t%3.1f\t%3.1f|\t|%3.1f|\n", tempDist(3), tempCov(3,1), tempCov(3,2), tempCov(3,3), tempDist(3));
             */
            /*
             if(normedDistance > 3){
             logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::MMSE normed distance for filters %d and %d is greater than 3 at %0.2f\n", filterIndex, secondFilterIndex, normedDistance);
             }
             */
        }
    }

    //weight ratio metric calculations
    //high weight particle (of filter 1) weight ratio to same locations of filter 2
    double highWeightParticleWeightRatio[MAX_NUM_FILTERS][MAX_NUM_FILTERS];
    double sumWeightsInFilterOne = 0;
    double sumWeightsInFilterTwo = 0;
    for(int filterIndex = 0; filterIndex < numFilters; filterIndex++){
        for(int secondFilterIndex = 0; secondFilterIndex < numFilters; secondFilterIndex++){
            if(secondFilterIndex == filterIndex){
                continue;
            }
            for(int indexP = 0; indexP < nParticles; indexP++){
                if(this->weights[filterIndex].weights[indexP] >= 1.0/nParticles){
                    //calculate weight agreement
                    sumWeightsInFilterOne += weights[filterIndex].weights[indexP];
                    sumWeightsInFilterTwo += weights[secondFilterIndex].weights[indexP];
                }
                //Earth Movers Distance
                //	~150,000 element simplex problem after size reducing approximations
            }

            //log results of filter comparisons
            if(sumWeightsInFilterOne >0){
                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::ratio of weights for high weight particles in filter %d and %d is %0.2f\n", filterIndex, secondFilterIndex, sumWeightsInFilterTwo/sumWeightsInFilterOne);
                highWeightParticleWeightRatio[filterIndex][secondFilterIndex] = sumWeightsInFilterTwo/sumWeightsInFilterOne;
            }else{
                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::ratio of weights for high weight particles -- filter %d had non-positive sumWeights: %0.2f", filterIndex, sumWeightsInFilterOne);
                highWeightParticleWeightRatio[filterIndex][secondFilterIndex] = 0;
            }

        }
    }

    //filter agreement calculations
    int agreementCounts[MAX_NUM_FILTERS];
    for(int index=0; index < this->numFilters; index++){agreementCounts[index] = 0;}
    int nonDisagreementCounts[MAX_NUM_FILTERS];
    for(int index=0; index < this->numFilters; index++){nonDisagreementCounts[index] = 0;}
    int minAgreements = 0;
    int bestIndex = -1;

    switch(this->filterComparison){
        case 0://use neither; return tightest converged (trace of covariance)
            *mmsePose = mmseArray[0];
            for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
                if(mmseArray[filterIndex].covariance[0] + mmseArray[filterIndex].covariance[2] < mmsePose->covariance[0] + mmsePose->covariance[2]){
                    *mmsePose = mmseArray[filterIndex];
                    bestIndex = filterIndex;
                }
            }
            logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::mmse selected from filter %d\n", bestIndex);
            break;

        case 1://use Gaussian Approximation normed distance
            for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
                for(int secondFilterIndex = filterIndex+1; secondFilterIndex < this->numFilters; secondFilterIndex++){
                    if(gaussianFilterDistances[filterIndex][secondFilterIndex] <= this->filterComparisonGaussianDistanceThreshold){
                        if((mmseArray[filterIndex].covariance[0] < 50) & (mmseArray[filterIndex].covariance[2] < 50) &
                           (mmseArray[secondFilterIndex].covariance[0] < 50) & (mmseArray[secondFilterIndex].covariance[2] < 50)){// both converged

                            agreementCounts[filterIndex]++;
                            agreementCounts[secondFilterIndex]++;
                        }else{//not converged enough to count as agreement
                            nonDisagreementCounts[filterIndex] ++;
                            nonDisagreementCounts[secondFilterIndex]++;
                        }
                    }
                }

                //track filter with the most agreements
                if(agreementCounts[filterIndex] > minAgreements){//return the filter with the most agreements
                    minAgreements = agreementCounts[filterIndex];
                    *mmsePose = mmseArray[filterIndex];
                    bestIndex = filterIndex;
                }else if(agreementCounts[filterIndex] == minAgreements){//in a tie, return the one with the tightest trace of the covariance
                    if(mmseArray[filterIndex].covariance[0] + mmseArray[filterIndex].covariance[2] < mmsePose->covariance[0] + mmsePose->covariance[2]){
                        *mmsePose = mmseArray[filterIndex];
                        bestIndex = filterIndex;
                    }
                }
            }

            //return the mmse from the filter with the most converged agreements
            if(minAgreements >0){
                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::mmse selected from filter %d with agreement count: %d\n", bestIndex, minAgreements);
            }else{
                //or the most un-converged agreements
                int minNonDisagreementCount = 0;
                for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
                    if(nonDisagreementCounts[filterIndex] > minNonDisagreementCount){
                        minNonDisagreementCount = nonDisagreementCounts[filterIndex];
                        *mmsePose = mmseArray[filterIndex];
                        bestIndex = filterIndex;
                    }else if(nonDisagreementCounts[filterIndex] == minNonDisagreementCount){
                        if(mmseArray[filterIndex].covariance[0] + mmseArray[filterIndex].covariance[2] < mmsePose->covariance[0] + mmsePose->covariance[2]){
                            *mmsePose = mmseArray[filterIndex];
                            bestIndex = filterIndex;
                        }
                    }
                }
                if(minNonDisagreementCount > 0){
                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::mmse selected from filter %d with unconverged agreement count: %d\n", bestIndex, minNonDisagreementCount);
                }else{
                    *mmsePose = *(this->lastNavPose);
                    mmsePose->covariance[0] = 10000;
                    mmsePose->covariance[2] = 10000;
                    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::no agreement; returning last nav pose\n");
                }
            }
            break;

        case 2://use weight ratio
            for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
                for(int secondFilterIndex = filterIndex+1; secondFilterIndex < this->numFilters; secondFilterIndex++){
                    if(highWeightParticleWeightRatio[filterIndex][secondFilterIndex] >= this->filterComparisonWeightRatioThreshold){
                        agreementCounts[filterIndex]++;
                    }
                    if(highWeightParticleWeightRatio[secondFilterIndex][filterIndex] >= this->filterComparisonWeightRatioThreshold){
                        agreementCounts[secondFilterIndex]++;
                    }

                }

                //track filter with highest number of agreements
                if(agreementCounts[filterIndex] > minAgreements){//return the filter with the most agreements
                    minAgreements = agreementCounts[filterIndex];
                    *mmsePose = mmseArray[filterIndex];
                    bestIndex = filterIndex;
                }else if(agreementCounts[filterIndex] == minAgreements){//in a tie, return the one with the tightest trace of the covariance
                    if(mmseArray[filterIndex].covariance[0] + mmseArray[filterIndex].covariance[2] < mmsePose->covariance[0] + mmsePose->covariance[2]){
                        *mmsePose = mmseArray[filterIndex];
                        bestIndex = filterIndex;
                    }
                }

            }

            //return mmse from filter with most agreements
            if(minAgreements >0){
                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::mmse selected from filter %d with agreement count: %d\n", bestIndex, minAgreements);
            }else{
                *mmsePose = *(this->lastNavPose);
                mmsePose->covariance[0] = 10000;
                mmsePose->covariance[2] = 10000;
                logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::no agreement; returning last nav pose\n");
            }
            break;

        default:
            *mmsePose = mmseArray[0];
            break;
    }
}

void
TNavBankFilter::
computeMMSE(poseT* mmsePose, int filterIndex){
    //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG), "TNavBF::MMSE filter#: %d\n", filterIndex);
    double sumWeights = 0;
    poseT tempPose;

    for(int i = 0; i < nParticles; i++) {
        double weight = this->weights[filterIndex].weights[i];
        sumWeights += weight;
        tempPose.x += weight * allParticles[i].position[0];
        tempPose.y += weight * allParticles[i].position[1];
        tempPose.z += weight * (allParticles[i].position[2] + this->depthBias[filterIndex][i]);
        tempPose.phi += weight * allParticles[i].attitude[0];
        tempPose.theta += weight * allParticles[i].attitude[1];
        //
        // Psi is in the berg frame (same as vehicle if the
        // SEARCH_PSI_BERG flag is not set) so we didn't change
        // anything here.
        if(SEARCH_COMPASS_BIAS) {
            tempPose.psi += weight * (allParticles[i].attitude[2] + allParticles[i].compassBias);
        } else {
            tempPose.psi += weight * allParticles[i].attitude[2];
        }
        if(SEARCH_PSI_BERG)
        {
            tempPose.psi_berg += weight * allParticles[i].psiBerg;
            //
            // Reports x,y,z in berg relative. Reports phi, theta, psi
            // of the vehicle wrto inertial, and reports psi_berg.
        }
        if(SEARCH_GYRO_BIAS) {
            tempPose.wy += weight * allParticles[i].gyroBias[0];
            tempPose.wz += weight * allParticles[i].gyroBias[1];
        }

    }

    if(sumWeights != 1) {
        tempPose.x /= sumWeights;
        tempPose.y /= sumWeights;
        tempPose.z /= sumWeights;
        tempPose.phi /= sumWeights;
        tempPose.theta /= sumWeights;
        tempPose.psi /= sumWeights;
        if(SEARCH_PSI_BERG)
        {
            tempPose.psi_berg /= sumWeights;
        }
        if(SEARCH_GYRO_BIAS) {
            tempPose.wy /= sumWeights;
            tempPose.wz /= sumWeights;
        }
    }

    for(int i = 0; i < nParticles; i++) {
        double weight = this->weights[filterIndex].weights[i];
        double alpha = weight / sumWeights;
        double temp1 = allParticles[i].position[0] - tempPose.x;
        tempPose.covariance[0] += temp1 * temp1 * alpha;
        double temp2 = allParticles[i].position[1] - tempPose.y;
        tempPose.covariance[2] += temp2 * temp2 * alpha;
        tempPose.covariance[1] += temp1 * temp2 * alpha;
        temp1 = allParticles[i].position[2] + this->depthBias[filterIndex][i] - tempPose.z;
        tempPose.covariance[5] += temp1 * temp1 * alpha;
        temp1 = allParticles[i].attitude[0] - tempPose.phi;
        tempPose.covariance[9] += temp1 * temp1 * alpha;
        temp1 = allParticles[i].attitude[1] - tempPose.theta;
        tempPose.covariance[14] += temp1 * temp1 * alpha;
        if(SEARCH_COMPASS_BIAS) {
            temp1 = allParticles[i].attitude[2] + allParticles[i].compassBias - tempPose.psi;
        } else {
            temp1 = allParticles[i].attitude[2] - tempPose.psi;
        }
        tempPose.covariance[20] += temp1 * temp1 * alpha;
        if(SEARCH_GYRO_BIAS) {
            temp1 = allParticles[i].gyroBias[0] - tempPose.wy;
            tempPose.covariance[27] += temp1 * temp1 * alpha;
            temp1 = allParticles[i].gyroBias[1] - tempPose.wz;
            tempPose.covariance[35] += temp1 * temp1 * alpha;
        }
        if( SEARCH_PSI_BERG)
        {
            temp1 = allParticles[i].psiBerg - tempPose.psi_berg;
            tempPose.covariance[44] += temp1 * temp1 * alpha;
        }
    }

    tempPose.covariance[5] += this->depthBiasCov[filterIndex];
    //      logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"\n variance(psi_berg) = %f\n",tempPose.covariance[44]);

    tempPose.time = this->lastNavPose->time;
    *mmsePose = tempPose;

    return;
}

//********************************************************************************

void
TNavBankFilter::
checkConvergence() {
    double kl;

    kl = computeKLdiv_gaussian_particles();

    //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Current KL: %.10f\n", kl);
    if(kl < 1) {
        converged = true;
    } else {
        converged = false;
    }
}

//********************************************************************************

int
TNavBankFilter::
defineAndLoadSubMap(const Matrix& beamsVF) {
    poseT mmseEst;
    double maxAttitude[3];
    Matrix beamsMF;
    double max_dx, max_dy;
    double widthPhi, widthTheta;
    double Nmin, Nmax, Emin, Emax;
    double mapSearch[2] = {0, 0};
    double numXdesired, numYdesired;
    int mapStatus;

    //determine current particle spread
    computeMMSE(&mmseEst);

    //check that variances are non-zero
    if(mmseEst.covariance[4] > 0.001 * PI / 180.0) {
        widthPhi = sqrt(mmseEst.covariance[4]);
    } else {
        widthPhi = 0.0;
    }

    if(mmseEst.covariance[5] > 0.001 * PI / 180.0) {
        widthTheta = sqrt(mmseEst.covariance[5]);
    } else {
        widthTheta = 0.0;
    }

    //determine max attitude angles from current particle spread
    maxAttitude[0] = max(fabs(mmseEst.phi + 3.0 * widthPhi),
                         fabs(mmseEst.phi - 3.0 * widthPhi));
    maxAttitude[1] = max(fabs(mmseEst.theta + 3.0 * widthTheta),
                         fabs(mmseEst.theta - 3.0 * widthTheta));
    maxAttitude[2] = mmseEst.psi;

    //determine beam projection for maximum attitude
    beamsMF = applyRotation(maxAttitude, beamsVF);

    //find maximum projection of beams
    max_dx = 0;
    max_dy = 0;

    for(int i = 0; i < beamsMF.Ncols(); i++) {
        double dx = fabs(beamsMF(1, i + 1));
        if(dx > max_dx) {
            max_dx = dx;
        }

        double dy = fabs(beamsMF(2, i + 1));
        if(dy > max_dy) {
            max_dy = dy;
        }
    }

    //find maximum particle North/East locations
    getDistBounds(Nmin, Nmax, Emin, Emax);

    //load submap based on max particle locations and projections.
    numXdesired = (Nmax - Nmin) / 2.0 + 1.5 * max_dx +//numXdesired might have nothing to do with the NUMBER desired and instead be the sidelength
    2 * fabs(terrainMap->Getdx());
    numYdesired = (Emax - Emin) / 2.0 + 1.5 * max_dy +
    2 * fabs(terrainMap->Getdy());

    mapSearch[0] = 2.0 * numXdesired;
    mapSearch[1] = 2.0 * numYdesired;

    //ask to extract a map based on vehicle location and search bounds
    mapStatus = terrainMap->loadSubMap((Nmax - Nmin) / 2.0 + Nmin, (Emax - Emin) / 2.0 + Emin, mapSearch,
                                       navData_x_, navData_y_);

    //return MAPBOUNDS_OUT_OF_BOUNDS;
    return mapStatus;
}


//********************************************************************************

void
TNavBankFilter::
saveCurrDistrib(ofstream& outputFile) {
    //check that the desired file exists and is open for writing
    if(!outputFile.is_open()) {
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Error:Tried to write to an unopened file."
             " Ignoring write command.");
        return;
    }

    if (PARTICLESTOFILE == _distribType) {
        writeParticlesToFile(allParticles, outputFile);
    } else {
        writeHistDistribToFile(allParticles, outputFile);
    }
}


particleT*
TNavBankFilter::
getParticles() {
    return this->allParticles;
}

//********************************************************************************

void
TNavBankFilter::
saveCurrParticles(ofstream& outputFile) {
    //check that the desired file exists and is open for writing
    if(!outputFile.is_open()) {
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Error:Tried to write to an unopened file."
             " Ignoring write command.");
        return;
    }

    writeParticlesToFile(allParticles, outputFile);
}

//********************************************************************************


void
TNavBankFilter::
initVariables() {
    //initialize particle array pointers
    allParticles = particleArray1;
    resampParticles = particleArray2;

    //The particle filter will start out with the maximum number of particles
    //nParticles = 3721; //61x61 array //MAX_PARTICLES;

    //Initialize counter for soundings used in correlation
    nSoundings = 0;

    //Initialize the mapPlotted flag to zero
    mapPlotted = 0;

    //Set resampled flag to false
    resampled = false;

    //Initialize Augmented MCL parameters
    if(USE_AUG_MCL) {
        a_slow = 0.03;
        a_fast = 0.2;
        w_slow = 0.0;
        w_fast = 0.0;
    }

    //Initialize save directories
    if(saveDirectory != NULL) {
        //load data files for storing results
        char fileName[2048];
        allParticlesFile.open(charCat(fileName, saveDirectory, "randParticles.txt"));
        resampParticlesFile.open(charCat(fileName, saveDirectory,
                                         "resampParticles.txt"));
        homerParticlesFile.open(charCat(fileName, saveDirectory, "homerParticles.txt"));
        homerMmseFile.open(charCat(fileName, saveDirectory, "homerMmse.txt"));
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Opening homerMmse %s", fileName);
        measWeightsFile.open(charCat(fileName, saveDirectory, "measWeights.txt"));
    }
    this->SubcloudNIS = 0;

    // Initialize parameters to defaults
    //
    this->numFilters = NUM_FILTERS;
    // allocate filter logs (to be consistent with numFilters)
    allocateLogs();
    this->PmfGridResolution = PMF_GRID_RESOLUTION;
    this->PmfGridSize = PMF_GRID_SIZE;
    this->nParticles = PMF_GRID_SIZE * PMF_GRID_SIZE;

    this->filterComparisonGaussianDistanceThreshold = 3.0;
    this->filterComparisonWeightRatioThreshold = 0.2;
    this->filterComparison = 0;
    this->filterConfiguration = 0;

    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::nParticles: %d\tnumFilters: %d\tPMF Grid Resolution: %0.1f\tPMF Grid Size: %d\n",
         this->nParticles, this->numFilters, this->PmfGridResolution, this->PmfGridSize);


    if(this->nParticles > MAX_NUM_PARTICLES){
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::nParticles > MAX_NUM_PARTICLES \n");
        this->nParticles = MAX_NUM_PARTICLES;
    }
    if(this->numFilters > MAX_NUM_FILTERS){
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF::nParticles > MAX_NUM_FILTERS \n");
        this->numFilters = MAX_NUM_FILTERS;
    }

}

//********************************************************************************

void
TNavBankFilter::
initParticleDist(const particleT& initialGuess) {
    //SymmetricMatrix tempCov(9);
    //SymmetricMatrix tempCovSqrt(9);
    //tempCovSqrt = 0.0;
    //double tempX, tempY;
    fstream particleFile; //Used for loading particles from a specified file

    /*
     tempCov.Row(1) << initWindowVar[0];
     tempCov.Row(2) << initWindowVar[1] << initWindowVar[2];
     tempCov.Row(3) << 0.0 << 0.0 << initWindowVar[5];
     tempCov.Row(4) << 0.0 << 0.0 << 0.0 << initWindowVar[9];
     tempCov.Row(5) << 0.0 << 0.0 << 0.0 << 0.0 << initWindowVar[14];
     tempCov.Row(6) << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << initWindowVar[20];
     tempCov.Row(7) << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << initWindowVar[27];
     tempCov.Row(8) << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << initWindowVar[35];
     tempCov.Row(9) << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << 0.0 << initWindowVar[44];
     tempCovSqrt.SymSubMatrix(1, 2) << computeMatrixSqrt(tempCov.SymSubMatrix(1, 2));

     tempCovSqrt(3, 3) = fabs(sqrt(initWindowVar[5]));
     tempCovSqrt(4, 4) = fabs(sqrt(initWindowVar[9]));
     tempCovSqrt(5, 5) = fabs(sqrt(initWindowVar[14]));
     tempCovSqrt(6, 6) = fabs(sqrt(initWindowVar[20]));
     tempCovSqrt(7, 7) = fabs(sqrt(initWindowVar[27]));
     tempCovSqrt(8, 8) = fabs(sqrt(initWindowVar[35]));
     tempCovSqrt(9, 9) = fabs(sqrt(initWindowVar[44]));
     */

    if(USE_PARTICLE_FILE){ //Specify starting particle locations in particles.cfg
        char temp[TRN_MAX_BEAMS]={0};
        // caller own name string, so must free it
        char *pfname=TNavConfig::instance()->getParticlesFile();
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF: Opening particles in %s\n",pfname);
        particleFile.open(pfname);
        if(particleFile.is_open()){
            //Read the number of particles in the file
            particleFile.getline(temp,10); //First line is the total number of particles in the file

            /*nParticles = atoi(temp);
             if(nParticles > MAX_PARTICLES){
             nParticles = MAX_PARTICLES;
             }*/

            //Read the number of states the file is giving
            particleFile.getline(temp,10);
            int nStates = atoi(temp);

            for(int i = 0; i < nParticles; i++) {
                //The file is structured such that the states are specified in the file as North, East, Down (optional), psiBerg (optional)
                allParticles[i] = initialGuess;
                for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
                    this->weights[filterIndex].weights[i] = 1.0 / nParticles;
                }

                particleFile.getline(temp,20,',');
                this->allParticles[i].position[0] = atof(temp);

                particleFile.getline(temp,20,',');
                this->allParticles[i].position[1] = atof(temp);
                if(nStates > 2){
                    particleFile.getline(temp,20, ',');
                    this->allParticles[i].position[2] = atof(temp);
                }
                // Estimation of psiBerg
                if(nStates > 3){
                    particleFile.getline(temp,20); //particleFile.getline(temp,20, ','); //If not the final state specified
                    double psiVehInBergFrame = atof(temp);
                    this->allParticles[i].psiBerg = this->allParticles[i].attitude[2] - psiVehInBergFrame;
                }
                /*To add another state to the file
                 if(nStates > 3){
                 particleFile.getline(temp,20); //particleFile.getline(temp,20, ','); //If not the final state specified
                 this->allParticles[i].newState = atof(temp);
                 }
                 */

            }
        }else{
            snprintf(temp, sizeof(temp), 
                    "TNavBankFilter::initParticleDist() - Error opening file: %s\n",
                    (pfname?pfname:"NULL"));
            if(pfname!=NULL)free(pfname);
            fprintf(stderr, "%s", temp);
            throw Exception(temp);
        }
        particleFile.close();
        if(pfname!=NULL)free(pfname);

    }else{
        // If not using the particle file:
        //Initialize the particle distribution to a gaussian around the initial guess
        //Variances defined in particleFilterDefs.h

        for(int i = 0; i < nParticles; i++) {
            allParticles[i] = initialGuess;
            for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
                this->weights[filterIndex].weights[i] = 1.0 / nParticles;
            }

            //initialize positions with uniform distribution (or reinitialize with gaussian)

            /*old fixed 61x61 grid
             this->allParticles[i].position[0] += (i/61)*2-60;//120m x 120m area at 2m spacing with 3721 (61x61) particles
             this->allParticles[i].position[1] += (i%61)*2-60;
             */

            this->allParticles[i].position[0] += (i/this->PmfGridSize)*this->PmfGridResolution - (this->PmfGridSize - 1.0)/2.0 * this->PmfGridResolution;
            this->allParticles[i].position[1] += (i%this->PmfGridSize)*this->PmfGridResolution - (this->PmfGridSize - 1.0)/2.0 * this->PmfGridResolution;

            //printf("%f\t%f\n",(i/this->PmfGridSize)*this->PmfGridResolution - (this->PmfGridSize - 1.0)/2.0 * this->PmfGridResolution, (i%this->PmfGridSize)*this->PmfGridResolution - (this->PmfGridSize - 1.0)/2.0 * this->PmfGridResolution);

            this->allParticles[i].position[2] += 0;

        }

    }

    //set size for each weight array
    for(int filterIndex = 0; filterIndex < this->numFilters; filterIndex++){
        this->weights[filterIndex].size = this->nParticles;
    }

    for(int filterIndex = 0; filterIndex < numFilters; filterIndex++){
        for(int i = 0; i < nParticles; i++) {
            //old this->allParticles[i].windowIndex = 0;
            this->windowIndex[filterIndex][i] = 0;
            for(int indexW = 0; indexW < 20; indexW ++){
                //old this->allParticles[i].windowedNis[indexW] = 0;
                this->windowedNis[filterIndex][i][indexW] = 0;
            }
        }
    }

    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TNavBF: Successfully initialized particles \n");
    return;
}

//********************************************************************************

void
TNavBankFilter::
attitudeMeasUpdate(const poseT& currPose) {
    double sumWeights = 0;
    // double sumSquaresWeights = 0;
    // double effSampSize;
    double phiVar = DPHI_STDDEV * DPHI_STDDEV;
    double thetaVar = DTHETA_STDDEV * DTHETA_STDDEV;

    //Loop through & apply measurement update to all particles
    for(int i = 0; i < nParticles; i++) {
        double sumSquaredError = (1.0 / phiVar) * pow(allParticles[i].attitude[0] - currPose.phi, 2)
        + (1.0 / thetaVar) * pow(allParticles[i].attitude[1] - currPose.theta, 2);
        double measProb = pow(2 * PI, -1.0) * pow(thetaVar * phiVar, -0.5) *
        exp(-0.5 * sumSquaredError);
        this->weights[0].weights[i] *= measProb;
        sumWeights += this->weights[0].weights[i];
    }
    //Normalize the distribution
    for(int i = 0; i < nParticles; i++) {
        this->weights[0].weights[i] /= sumWeights;
        //		sumSquaresWeights += pow(this->weights[0].weights[i], 2);
    }
    //effSampSize = 1.0 / sumSquaresWeights;

    if(USE_AUG_MCL) {
        //Update Augmented MCL average weight parameters
        double avgWeights = sumWeights / nParticles;
        if(w_slow == 0 && w_fast == 0) {
            w_slow = avgWeights;
            w_fast = avgWeights;
        }
        w_slow += a_slow * (avgWeights - w_slow);
        w_fast += a_fast * (avgWeights - w_fast);
    }

    //Resample the distribution if appropriate
    /*if(effSampSize < MIN_EFF_SAMP_SIZE * nParticles) {
     //plot current particle distribution if Matlab is set to be used
     #ifdef USE_MATLAB
     mapT mapForPloting;
     terrainMap->GetMapT(mapForPloting);
     plotMapMatlab(mapForPloting.depths,
     mapForPloting.xpts,
     mapForPloting.ypts,
     "title('Sub-Map and Particle Distribution: Prior to Resampling');",
     "figure(2)");

     plotParticleDistMatlab(allParticles, "figure(2)");
     #endif

     //resample particle distribution
     resampParticleDist();
     resampled = true;
     } else {
     resampled = false;
     }*/

}

//********************************************************************************

bool
TNavBankFilter::
homerMeasUpdate(const measT& currMeas) {
    double homerPoseN[MAX_PARTICLES];
    double homerPoseE[MAX_PARTICLES];
    double homerPoseMu[2] = {0, 0};
    double homerPoseCov[3] = {0, 0, 0};
    double homerRelPose[3] = {currMeas.alongTrack[0], currMeas.crossTrack[0],
        currMeas.altitudes[0]
    };
    Matrix homerInertPose(3, 1);
    homerInertPose = 0.0;
    double sumWeights = 0;
    double range_stddev[3] = {fabs(homerRelPose[0])* HOMER_RANGE_PER_ERROR / 100.0, fabs(homerRelPose[1])* HOMER_RANGE_PER_ERROR / 100.0,
        fabs(homerRelPose[2])* HOMER_RANGE_PER_ERROR / 100.0
    };
    Matrix currHomerPose(3, 1);

    //Compute homer locations in inertial space from particles
    for(int i = 0; i < nParticles; i++) {
        //add uncertainty to homer measurement based on 2.75% range error
        currHomerPose(1, 1) = homerRelPose[0] + randn_zeroMean(range_stddev[0]);
        currHomerPose(2, 1) = homerRelPose[1] + randn_zeroMean(range_stddev[1]);
        currHomerPose(3, 1) = homerRelPose[2] + randn_zeroMean(range_stddev[2]);

        //rotate rel pose into inertial coordinates
        homerInertPose = applyRotation(allParticles[i].attitude, currHomerPose);

        //fill in homer pose based on particle vehicle pose estimate
        homerPoseN[i] = allParticles[i].position[0] + homerInertPose(1, 1);
        homerPoseE[i] = allParticles[i].position[1] + homerInertPose(2, 1);
    }

    //Compute mean and variance of current homer pose estimate
    for(int i = 0; i < nParticles; i++) {
        double weight = this->weights[0].weights[i];
        sumWeights += weight;

        homerPoseMu[0] += weight * homerPoseN[i];
        homerPoseMu[1] += weight * homerPoseE[i];
    }

    if(sumWeights != 1) {
        homerPoseMu[0] /= sumWeights;
        homerPoseMu[1] /= sumWeights;
    }
    for(int i = 0; i < nParticles; i++) {
        double weight = this->weights[0].weights[i];
        double alpha = weight / sumWeights;
        double temp1 = homerPoseN[i] - homerPoseMu[0];
        homerPoseCov[0] += temp1 * temp1 * alpha;
        double temp2 = homerPoseE[i] - homerPoseMu[1];
        homerPoseCov[1] += temp2 * temp2 * alpha;
        homerPoseCov[2] += temp1 * temp2 * alpha;
    }

    //Write homer estimates to file

    //check that the desired file exists and is open for writing
    if(!homerParticlesFile.is_open()) {
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Error:Tried to write homer location particle data to an unopened file."
             " Ignoring write command.");
        return false;
    } else {
        if(SAVE_PARTICLES) {
            for(int i = 0; i < nParticles; i++)
            homerParticlesFile << setprecision(15) << i << "\t" << this->weights[0].weights[i] << "\t"
            << homerPoseN[i] << "\t" << homerPoseE[i] << endl;
        }
    }

    //check that the desired file exists and is open for writing
    if(!homerMmseFile.is_open()) {
        logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Error:Tried to write homer location mmse data to an unopened file."
             " Ignoring write command.");
        return false;
    } else {
        homerMmseFile << setprecision(15) << currMeas.time << "\t" << homerPoseMu[0]
        << "\t" << homerPoseMu[1] << "\t" << homerPoseCov[0] << "\t"
        << homerPoseCov[1] << "\t" << homerPoseCov[2] << endl;
    }


    return true;
}

//********************************************************************************

bool
TNavBankFilter::
treatBeamAsNan(int filterIndex, Matrix beamIF, const particleT &particle){
    //printf("%f\t%f\t%f\n", beamIF(1,1), beamIF(2,1), beamIF(3,1));
    switch(this->filterConfiguration){
        case 0: //1 filter
        case 1: //2 filter beam based
        case 2: // three filter beam based
            return false;
            break;


        case 3: // 6x 20m overlapping stripes; 3 N-S, 3 E-W
            return ((int)((10*filterIndex + beamIF(1,1) + particle.position[0]) * (filterIndex<3) + (10*filterIndex + beamIF(2,1) + particle.position[1]) * (filterIndex>=3)) % 30) >= 10;
            break;
        case 4:// 6x 10m non-overlapping stripes; 3 N-S, 3 E-W
            return ((int)((10*filterIndex + beamIF(1,1) + particle.position[0]) * (filterIndex<3) + (10*filterIndex + beamIF(2,1) + particle.position[1]) * (filterIndex>=3)) % 30) < 10;
            break;
        default:
            return false;
            break;


            //three along track stripes
            //3 sets of along track stripes (10m wide each)
            //up to 11 sets of tiles (cardinal, 20m?)


    }
    return false;
}
//*****************************************************/
bool
TNavBankFilter::
getExpectedMeasDiffParticle(particleT& particle, const Matrix& beamsSF, double* beamRanges, const int* beamIndices, double& mapVar) {
    //Update Expected Measurement Differences
    // This function takes in a particle (particle) and the beams in the ??? frame
    // (beamsSF), and the ranges (beamRanges)
    //
    // It extracts the difference between the measurement and expected measurement
    // for the particle, using either and octree map (this->mapType==2), or a DEM with either
    // ray tracing (USE_RANGE_CORR) or the standard projection method (NOTHING SELECTED)
    //
    // It also outputs the map variance (mapVar) that is also later used with particle
    // weighting


#if 0
    // RGH: I don't think this needs to be initialized every time since we're
    // either setting tempUseBeam[i] true or false for all Ncols beams
    if (0)
        for(int index = 0; index < 120; index++){
            this->tempUseBeam[index] = true;//default use the beam unless there are problems
            //usebeam first n entries correspond to the n real measurements passed in; higher indices have no meaning. e.g. first 4 entries for DVL.
        }
#endif

    //!double beamN, beamE, beamZ, mapZ;
    //  double mapVar = 1;
    Matrix beamsMF;
    Matrix beamsVF;

    //!double beamU[3];		//Used for octree, range
    double beamVector[3];
    //float estRange;
    //!double r_pred;		//range
    std::vector<double> tempExpectedMeasDiff(beamsSF.Ncols(), 0);


    //If searching over alignment state, first bring beams into vehicle frame
    if(SEARCH_ALIGN_STATE) {
        double currDvlAttitude[3] = {dvlAttitude[0], dvlAttitude[1],
            dvlAttitude[2]
        };
        currDvlAttitude[0] += particle.alignState[0];
        currDvlAttitude[1] += particle.alignState[1];
        currDvlAttitude[2] += particle.alignState[2];
        beamsVF = applyRotation(currDvlAttitude, beamsSF);
    } else {
        beamsVF = beamsSF;
    }

    //Rotate the beams from the vehicle frame to the map frame
    if(ALLOW_ATTITUDE_SEARCH) {
        double currAttitude[3] = {particle.attitude[0], particle.attitude[1],
            particle.attitude[2]
        };
        if(SEARCH_COMPASS_BIAS) {
            currAttitude[2] += particle.compassBias;
        }

        beamsMF = applyRotation(currAttitude, beamsVF);
    } else {
        beamsMF = beamsVF;
    }


    // Return value: false if no beams should be used; otherwise true.
    //
    bool goodBeams = false;

    for(int i = 0; i < beamsMF.Ncols(); i++) {
        beamVector[0] = beamsMF(1, i + 1);//the directionVector
        beamVector[1] = beamsMF(2, i + 1);
        beamVector[2] = beamsMF(3, i + 1);

        tempExpectedMeasDiff[i] =
        terrainMap->GetRangeError(mapVar, particle.position, beamVector, beamRanges[beamIndices[i]]);

        // if(isnan(tempExpectedMeasDiff[i])){
        if(ISNIN(tempExpectedMeasDiff[i])){
            //tempExpectedMeasDiff[i] = 0;
            this->tempUseBeam[i] = false; //beam hit map hole or missed -> don't use this beam to compare particles
            /*if(!USE_MAP_NAN){
             return false;
             }
             //ADD IN CODE TO HANDLE NAN VALUES*/
        }
        else
        {
            this->tempUseBeam[i] = true;
            goodBeams = true;            // OK, at least one beam is good
        }

    }

    particle.expectedMeasDiff = tempExpectedMeasDiff;
    return goodBeams;
}

//********************************************************************************

// Assume that diffPose is inertially referenced.  diffPose.psi is inertial heading change.

void
TNavBankFilter::
motionUpdateParticle(particleT& particle, const poseT& diffPose) {
    double vehicleDisp[3];

    //double psiDot, phiDot, thetaDot;
    Matrix velocity_sf(3, 1);
    Matrix velocity_vf(3, 1);
    Matrix velocity_if(3, 1);
    /*Matrix accel_sf(3, 1);
     Matrix accel_vf(3, 1);
     Matrix accel_if(3, 1);
     //double driftStddev, cep;
     double cosTheta = cos(particle.attitude[1]);
     double sinPhi = sin(particle.attitude[0]);
     double cosPhi = cos(particle.attitude[0]);
     double tanTheta = tan(particle.attitude[1]);
     */

    //Depth update is given by INS delta z
    vehicleDisp[2] = diffPose.z;

    //If there is valid GPS data, use the stored INS pose information to perform
    // the motion update.  Otherwise, use the stored velocity data (from DVL) and
    // vehicle attitude information from the previous time step to perform the
    // motion update.
    if(diffPose.gpsValid || !DEAD_RECKON || !lastNavPose->dvlValid) {
        /*//Add Gaussian noise to account for uncertainty in the inertial disp.
         //measurement
         cep = (this->vehicle->driftRate / 100.0) * (sqrt(diffPose.x * diffPose.x + diffPose.y * diffPose.y));

         //TODO:  check conversion from CEP to Stddev, I don't feel like there should be a sqrt outside the CEP
         driftStddev = MOTION_NOISE_MULTIPLIER * sqrt(cep / sqrt(-2 * (log(1 - 0.5))));
         //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"MOTION UPDATE: CEP = %f\n",this->vehicle->driftRate/100.0); //TODO: Remove this (when no longer needed)
         */
        vehicleDisp[0] = diffPose.x;// + randn_zeroMean(driftStddev);
        vehicleDisp[1] = diffPose.y;// + randn_zeroMean(driftStddev);
        //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"standard navigation update...\n");
    } else {
        //Apply bias and scale factor corrections IFF DVL is returning ground
        //velocity and we are searching over dvl bias/scale factor

        velocity_sf(1, 1) = lastNavPose->vx;
        velocity_sf(2, 1) = lastNavPose->vy;
        velocity_sf(3, 1) = lastNavPose->vz;

        double currDvlAttitude[3] = {dvlAttitude[0], dvlAttitude[1],
            dvlAttitude[2]
        };
        double currAttitude[3] = {particle.attitude[0], particle.attitude[1],
            particle.attitude[2]
        };

        //Transform sensor frame velocities to vehicle frame:
        velocity_vf = applyRotation(currDvlAttitude, velocity_sf);

        velocity_if = applyRotation(currAttitude, velocity_vf);

        //Account for water velocity if DVL does not have bottom lock
        if(!lastNavPose->bottomLock) {
            velocity_if(1, 1) -= currentVel[0];
            velocity_if(2, 1) -= currentVel[1];
            velocity_if(3, 1) -= currentVel[2];
        }

        //Compute vehicle displacement based on inertial velocity
        vehicleDisp[0] = velocity_if(1, 1) * diffPose.time;
        vehicleDisp[1] = velocity_if(2, 1) * diffPose.time;

    }

    particle.position[0] += vehicleDisp[0];
    particle.position[1] += vehicleDisp[1];
    particle.position[2] += vehicleDisp[2];

    //Compute new heading of the particle

    // Not SEARCH_GYRO_BIAS
    particle.attitude[2] += diffPose.psi;


    //Compute the new roll/pitch of the particle
    if(!INTEG_PHI_THETA) {
        particle.attitude[0] += diffPose.phi;
        particle.attitude[1] += diffPose.theta;
    }


    return;
}

//********************************************************************************
/*
 void
 TNavBankFilter::
 resampParticleDist() {
 double step;
 int N, M = 0;
 poseT mmseEst;

 logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"TerrainNav::Resampling particle filter...\n");

 //If using Augmented MCL, add some purely random samples to the resampled
 //distribution
 if(USE_AUG_MCL) {
 //For a fraction of the particles based on average weights, add random
 //samples
 M = int(0.1 * (1.0 - w_fast / w_slow) * nParticles);
 if(M < 0) {
 M = 0;
 }

 //logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"number of random samples to generate: %i\n", M);
 computeMMSE(&mmseEst);
 for(int m = 0; m < M; m++) {
 resampParticles[m] = allParticles[0];
 resampParticles[m].weight = 1.0 / nParticles;
 }
 }
 N = nParticles - M;

 //Use the low-variance sampling algorithm as outlined in
 //"Probabilistic Robotics" by Thrun, Burgard, Fox - pg. 110
 step = 1.0 / N;
 double r = (rand() + 1) * (1.0 / (RAND_MAX + 1.0)) * 1.0 / nParticles;
 double c = allParticles[0].weight;
 for(int m = 0; m < N; m++) {
 int U = r + m * step;

 while(c < U) {
 i++;
 c += allParticles[i].weight;
 }

 //copy state of current particles to resampled particles
 resampParticles[m + M] = allParticles[i];
 resampParticles[m + M].weight = 1.0 / nParticles;
 }

 //if(saveDirectory != NULL)
 //   writeParticlesToFile(resampParticles, resampParticlesFile);

 //swap in resampled distribution
 updateParticleDist();
 }
 */
//********************************************************************************

void
TNavBankFilter::
getDistBounds(double& Nmin, double& Nmax, double& Emin, double& Emax) {

    Nmin = allParticles[0].position[0];
    Nmax = Nmin;
    Emin = allParticles[0].position[1];
    Emax = Emin;

    for(int i = 0; i < nParticles; i++) {
        Nmin = min(Nmin, allParticles[i].position[0]);
        Nmax = max(Nmax, allParticles[i].position[0]);
        Emin = min(Emin, allParticles[i].position[1]);
        Emax = max(Emax, allParticles[i].position[1]);
    }
    return;
}

//********************************************************************************

//********************************************************************************

void
TNavBankFilter::
getParticlePose(const particleT& particle, poseT* particlePose) {
    particlePose->x = particle.position[0];
    particlePose->y = particle.position[1];
    particlePose->z = particle.position[2];

    particlePose->phi = particle.attitude[0];
    particlePose->theta = particle.attitude[1];
    particlePose->psi = particle.attitude[2];

    if(SEARCH_COMPASS_BIAS) {
        particlePose->psi += particle.compassBias;
    }

    if(SEARCH_PSI_BERG)
    {
        particlePose->psi_berg = particle.psiBerg;
    }

    particlePose->time = this->lastNavPose->time;

    return;
}

//********************************************************************************

void
TNavBankFilter::
updateParticleDist() {
    //temporary pointer
    particleT* tempPointer;

    tempPointer = resampParticles;

    //swap pointers
    resampParticles = allParticles;
    allParticles = tempPointer;
}

//********************************************************************************

double
TNavBankFilter::
computeKLdiv_gaussian_particles() {
    Matrix Value(1, 1);
    ColumnVector dx(2);
    SymmetricMatrix Cov(2);
    Matrix A;
    double eta;
    double kl = 0;
    poseT mmseEst;
    double mu[2];

    //compute mean estimate
    computeMMSE(&mmseEst);

    mu[0] = mmseEst.x;
    mu[1] = mmseEst.y;
    Cov(1, 1) = mmseEst.covariance[0];
    Cov(2, 2) = mmseEst.covariance[1];
    Cov(2, 1) = mmseEst.covariance[2];

    //compute gaussian normalization factor
    A = 2.0 * PI * Cov;
    eta = pow(A.Determinant(), -0.5);

    //compute inverse of covariance for gaussian calculation
    A = Cov.i();

    //sum KL over all entries
    for(int i = 0; i < nParticles; i++) {
        dx(1) = allParticles[i].position[0] - mu[0];
        dx(2) = allParticles[i].position[1] - mu[1];

        //compute current gaussian probability
        Value = dx.t() * A * dx;
        double q = eta * exp(Value.AsScalar() * -0.5);

        //add current kl entry
        if(this->weights[0].weights[i] / q > 1e-50 && this->weights[0].weights[i] / q < 1e50) {
            kl += this->weights[0].weights[i] * log(this->weights[0].weights[i] / q);
        }
    }

    return kl;
}

//********************************************************************************

/*
 void
 TNavBankFilter::
 computeInnovationsMatrices(particleT* particles, SymmetricMatrix& measVarMat, ColumnVector& measDiffMean) {
 //This function computes the mean and variance matrices of the expected measurements
 //This is passed back in measVarMat and measDiffMean

 measDiffMean = 0.0;		//Set mean of expected measurement difference to zero
 measVarMat = 0.0;		//Set measurement variance matrix to zero


 //Calculate mean expected measurement difference
 for(int i = 0; i < nParticles; i++) {
 for(j = 0; j < measVarMat.Ncols(); j++) {
 measDiffMean(j + 1) += particles[i].expectedMeasDiff[j] * this->weights[0].weights[i];
 }
 }

 //Calculate Variance of expected measurement differences from the map
 for(int i = 0; i < nParticles; i++) {
 for(int j = 0; j < measVarMat.Ncols(); j++) {
 for(int k = j; k < measVarMat.Ncols(); k++) {
 measVarMat(k + 1, j + 1) += (particles[i].expectedMeasDiff[j] - measDiffMean(j + 1)) * (particles[i].expectedMeasDiff[k] -
 measDiffMean(k + 1)) * this->weights[0].weights[i];
 }
 }
 }

 }
 */
//********************************************************************************
void
TNavBankFilter::
plotMapMatlab(const Matrix& Surf, double* xpts, double* ypts, const char* plotTitle, const char* figureNum) {
#ifdef USE_MATLAB
    mxArray* Map = NULL;
    mxArray* MATLABxpts = NULL;
    mxArray* MATLABypts = NULL;
    mxArray* NavPoseX = NULL;
    mxArray* NavPoseY = NULL;

    //copy contents of Surf into Matlab variables Map, MATLABxpts, MATLABypts
    Map = mxCreateDoubleMatrix(Surf.Ncols(), Surf.Nrows(), mxREAL);
    memcpy((void*)mxGetPr(Map), (void*) Surf.Store(),
           Surf.Storage()*sizeof(double));
    engPutVariable(matlabEng, "Map", Map);

    MATLABxpts = mxCreateDoubleMatrix(1, Surf.Nrows(), mxREAL);
    memcpy((void*)mxGetPr(MATLABxpts), (void*)xpts, Surf.Nrows()*sizeof(double));
    engPutVariable(matlabEng, "xpts", MATLABxpts);

    MATLABypts = mxCreateDoubleMatrix(1, Surf.Ncols(), mxREAL);
    memcpy((void*)mxGetPr(MATLABypts), (void*)ypts, Surf.Ncols()*sizeof(double));
    engPutVariable(matlabEng, "ypts", MATLABypts);

    //copy nav pose information into Matlab variables navPoseX and navPoseY
    NavPoseX = mxCreateDoubleMatrix(1, 1, mxREAL);
    memcpy((void*)mxGetPr(NavPoseX), (void*) &lastNavPose->x, sizeof(double));
    engPutVariable(matlabEng, "navPoseX", NavPoseX);

    NavPoseY = mxCreateDoubleMatrix(1, 1, mxREAL);
    memcpy((void*)mxGetPr(NavPoseY), (void*) &lastNavPose->y, sizeof(double));
    engPutVariable(matlabEng, "navPoseY", NavPoseY);


    //plot the loaded submap surface
    engEvalString(matlabEng, figureNum);
    engEvalString(matlabEng, "hold off;");
    engEvalString(matlabEng, "imagesc(ypts-navPoseY,xpts-navPoseX, Map')");
    engEvalString(matlabEng, "ylabel('North (m)'); xlabel('East (m)')");
    engEvalString(matlabEng, "axis equal;");
    engEvalString(matlabEng, "set(gcf,'color','w');");
    engEvalString(matlabEng, "colorbar;");
    engEvalString(matlabEng, plotTitle);
    engEvalString(matlabEng, "hold on;");

    //remove memory in Matlab
    mxDestroyArray(Map);
    mxDestroyArray(MATLABxpts);
    mxDestroyArray(MATLABypts);
    mxDestroyArray(NavPoseX);
    mxDestroyArray(NavPoseY);

#else
    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Could not generate plot; Matlab is not set to be used");
#endif
}

//********************************************************************************

void
TNavBankFilter::
plotParticleDistMatlab(particleT* particles, const char* figureNum) {
#ifdef USE_MATLAB
    mxArray* matlabPart = NULL;
    Matrix allPart(nParticles, 3);

    //fill Matrix with particle pose and weight information
    for(int i = 0; i < nParticles; i++) {
        allPart(i + 1, 1) = particles[i].position[0] - lastNavPose->x;
        allPart(i + 1, 2) = particles[i].position[1] - lastNavPose->y;
        allPart(i + 1, 3) = this->weights[0].weights[i];
    }

    //put matrix into matlab
    matlabPart = mxCreateDoubleMatrix(3, nParticles, mxREAL);
    memcpy((void*)mxGetPr(matlabPart), (void*) allPart.Store(),
           allPart.Storage()*sizeof(double));
    engPutVariable(matlabEng, "A", matlabPart);

    //plot particle positions
    engEvalString(matlabEng, figureNum);
    engEvalString(matlabEng, "if(exist('h','var')); delete(h); end;");
    engEvalString(matlabEng, "scatter(A(2,:), A(1,:), A(3,:)*50000, 'w')");

    //remove memory in Matlab
    mxDestroyArray(matlabPart);
#else
    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Could not generate plot; Matlab is not set to be used");
#endif

}

//********************************************************************************

void
TNavBankFilter::
plotBeamMatlab(const double& npos, const double& epos, const double& zpos, const double& beamN,
               const double& beamE, const double& beamZ, const char* figureNum) {
#ifdef USE_MATLAB
    mxArray* beamParams = NULL;
    Matrix beamInfo(1, 6);

    beamInfo(1, 1) = npos;
    beamInfo(1, 2) = epos;
    beamInfo(1, 3) = zpos;
    beamInfo(1, 4) = beamN;
    beamInfo(1, 5) = beamE;
    beamInfo(1, 6) = beamZ;

    //put matrix into matlab
    beamParams = mxCreateDoubleMatrix(1, 6, mxREAL);
    memcpy((void*)mxGetPr(beamParams), (void*) beamInfo.Store(),
           beamInfo.Storage()*sizeof(double));
    engPutVariable(matlabEng, "beamParams", beamParams);

    //plot particle positions
    engEvalString(matlabEng, figureNum);
    engEvalString(matlabEng, "line([beamParams(1) beamParams(4)], [beamParams(2) beamParams(5)], [beamParams(3) beamParams(6)])");
    //engEvalString(matlabEng, "save /home/pkimball/latestRunWorkspace");

    //remove memory in Matlab
    mxDestroyArray(beamParams);
#else
    logs(TL_OMASK(TL_TNAV_BANK_FILTER, TL_LOG),"Could not generate plot; Matlab is not set to be used");
#endif
}

void
TNavBankFilter::
writeParticlesToFile(particleT* particles, ofstream& particlesFile) {

    //write particle information to file for all particles
    for(int i = 0; i < nParticles; i++) {
        particlesFile << setprecision(15) << i << "\t" << this->weights[0].weights[i] << "\t"
        << particles[i].position[0] << "\t" << particles[i].position[1]
        << "\t" << particles[i].position[2];
        if(ALLOW_ATTITUDE_SEARCH)
            particlesFile << setprecision(15) << "\t" << particles[i].attitude[0]
            << "\t" << particles[i].attitude[1]
            << "\t" << particles[i].attitude[2];
        if(MOVING_TERRAIN)
            particlesFile << setprecision(15) << "\t" << particles[i].terrainState[0]
            << "\t" << particles[i].terrainState[1]
            << "\t" << particles[i].terrainState[2];
        if(SEARCH_COMPASS_BIAS) {
            particlesFile << setprecision(15) << "\t" << particles[i].compassBias;
        }
        if(SEARCH_PSI_BERG) {
            particlesFile << setprecision(15) << "\t" << particles[i].psiBerg;
        }
        if(SEARCH_ALIGN_STATE)
            particlesFile << setprecision(15) << "\t" << particles[i].alignState[0]
            << "\t" << particles[i].alignState[1]
            << "\t" << particles[i].alignState[2];
        if(SEARCH_GYRO_BIAS) {
            particlesFile << setprecision(15) << "\t" << particles[i].gyroBias[0]
            << "\t" << particles[i].gyroBias[1];
            if(INTEG_PHI_THETA) {
                particlesFile << setprecision(15) << "\t" << particles[i].gyroBias[2];
            }
        }
        if(SEARCH_DVL_ERRORS)
            particlesFile << setprecision(15) << "\t" << particles[i].dvlScaleFactor
            << "\t" << particles[i].dvlBias[0]
            << "\t" << particles[i].dvlBias[1]
            << "\t" << particles[i].dvlBias[2];

        particlesFile << endl;

    }
}

//********************************************************************************

void
TNavBankFilter::
writeHistDistribToFile(particleT* particles, ofstream& particlesFile) {

    double Nmin, Nmax, Emin, Emax, Zmin, Zmax, Phmin, Phmax, Tmin, Tmax, Pmin, Pmax;
    double dN, dE, dZ, dPh, dT, dP;
    RowVector likeN, likeE, likeZ, likePh, likeT, likeP;
    int numN, numE, numZ, numPh, numT, numP;

    //compute histogram distributions for each filter state
    //find minimum/maximum particle North/East locations
    getDistBounds(Nmin, Nmax, Emin, Emax);

    //find minimum/maximum particle Depth and Heading
    Zmin = allParticles[0].position[2];
    Zmax = Zmin;
    Pmin = allParticles[0].attitude[2];
    if(SEARCH_COMPASS_BIAS) {
        Pmin += allParticles[0].compassBias;
    }
    if(SEARCH_PSI_BERG) {
        Pmin -= allParticles[0].psiBerg;
    }
    Pmax = Pmin;
    Phmin = allParticles[0].attitude[0];
    Phmax = Phmin;
    Tmin = allParticles[0].attitude[1];
    Tmax = Tmin;

    for(int i = 0; i < nParticles; i++) {
        Zmin = min(Zmin, allParticles[i].position[2]);
        Zmax = max(Zmax, allParticles[i].position[2]);
        Phmin = min(Phmin, allParticles[i].attitude[0]);
        Phmax = max(Phmax, allParticles[i].attitude[0]);
        Tmin = min(Tmin, allParticles[i].attitude[1]);
        Tmax = max(Tmax, allParticles[i].attitude[1]);
        if(SEARCH_COMPASS_BIAS) {
            Pmin = min(Pmin, allParticles[i].attitude[2] + allParticles[i].compassBias);
            Pmax = max(Pmax, allParticles[i].attitude[2] + allParticles[i].compassBias);
        } else if(SEARCH_PSI_BERG) {
            Pmin = min(Pmin, allParticles[i].attitude[2] - allParticles[i].psiBerg);
            Pmax = max(Pmax, allParticles[i].attitude[2] - allParticles[i].psiBerg);
        } else {
            Pmin = min(Pmin, allParticles[i].attitude[2]);
            Pmax = max(Pmax, allParticles[i].attitude[2]);
        }
    }

    dN = 0.1;
    dE = 0.1;
    dZ = 0.01;
    dP = 0.001;
    dPh = 0.001;
    dT = 0.001;

    numN = int((Nmax - Nmin + dN) / dN);
    numE = int((Emax - Emin + dE) / dE);
    numZ = int((Zmax - Zmin + dZ) / dZ);
    numPh = int((Phmax - Phmin + dPh) / dPh);
    numT = int((Tmax - Tmin + dT) / dT);
    numP = int((Pmax - Pmin + dP) / dP);

    //create histogram vectors
    likeN.ReSize(numN);
    likeE.ReSize(numE);
    likeZ.ReSize(numZ);
    likePh.ReSize(numPh);
    likeT.ReSize(numT);
    likeP.ReSize(numP);

    likeN = 0.0;
    likeE = 0.0;
    likeZ = 0.0;
    likePh = 0.0;
    likeT = 0.0;
    likeP = 0.0;

    //fill in histogram vectors
    for(int i = 0; i < nParticles; i++) {
        //fill in likeN
        int idx = closestPtUniformArray(particles[i].position[0], Nmin, Nmax,
                                        numN);
        likeN(idx + 1) += this->weights[0].weights[i];

        //fill in likeE
        idx = closestPtUniformArray(particles[i].position[1], Emin, Emax,
                                    numE);
        likeE(idx + 1) += this->weights[0].weights[i];

        //fill in likeZ
        idx = closestPtUniformArray(particles[i].position[2], Zmin, Zmax,
                                    numZ);
        likeZ(idx + 1) += this->weights[0].weights[i];

        //fill in likePh
        idx = closestPtUniformArray(particles[i].attitude[0], Phmin, Phmax,
                                    numPh);
        likePh(idx + 1) += this->weights[0].weights[i];

        //fill in likeT
        idx = closestPtUniformArray(particles[i].attitude[1], Tmin, Tmax,
                                    numT);
        likeT(idx + 1) += this->weights[0].weights[i];

        //fill in likeP
        if(SEARCH_COMPASS_BIAS)
        {
            idx = closestPtUniformArray(particles[i].attitude[2]
                                        + particles[i].compassBias, Pmin, Pmax,
                                        numP);
        }
        else if(SEARCH_PSI_BERG)
        {
            idx = closestPtUniformArray(particles[i].attitude[2]
                                        - particles[i].psiBerg, Pmin, Pmax,
                                        numP);
        }
        else
        {
            idx = closestPtUniformArray(particles[i].attitude[2], Pmin, Pmax,
                                        numP);
        }
        likeP(idx + 1) += this->weights[0].weights[i];
    }

    //write histogram information to file
    particlesFile << 1 << "\t" << Nmin << "\t" << Nmax << "\t" << numN << endl;
    particlesFile << likeN;
    particlesFile << 2 << "\t" << Emin << "\t" << Emax << "\t" << numE << endl;
    particlesFile << likeE;
    particlesFile << 3 << "\t" << Zmin << "\t" << Zmax << "\t" << numZ << endl;
    particlesFile << likeZ;
    particlesFile << 4 << "\t" << Phmin << "\t" << Phmax << "\t" << numPh << endl;
    particlesFile << likePh;
    particlesFile << 5 << "\t" << Tmin << "\t" << Tmax << "\t" << numT << endl;
    particlesFile << likeT;
    particlesFile << 6 << "\t" << Pmin << "\t" << Pmax << "\t" << numP << endl;
    particlesFile << likeP;

}
