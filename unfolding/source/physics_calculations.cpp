//**************************************************************************************************
// The functions included in this module aid in performing the physics and calculations necessary
// for the neutron unfolding program.
//**************************************************************************************************

#include "physics_calculations.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <random>
#include <cmath>
#include <stdlib.h>
#include <algorithm>
#include <vector>

// mt19937 is a random number generator class based on the Mersenne Twister algorithm
// Create an mt19937 object, called mrand, that is seeded with the current time in seconds
std::mt19937 mrand(std::time(0));

//==================================================================================================
// Return a 1D normalized vector of a system matrix used in MLEM-style reconstruction algorithms
//==================================================================================================
std::vector<double> normalizeResponse(int num_bins, int num_measurements, std::vector<std::vector<double>>& system_response)
{
    std::vector<double> normalized_vector;
    // Create the normalization factors to be applied to MLEM-estimated spectral values:
    //  - each element of f stores the sum of 8 elements of the transpose system (response)
    //    matrix. The 8 elements correspond to the relative contributions of each MLEM-estimated
    //    data point to the MLEM-estimated spectral value.
    for(int i_bin = 0; i_bin < num_bins; i_bin++)
    {
        double temp_value = 0;
        for(int i_meas = 0; i_meas < num_measurements; i_meas++)
        {
            temp_value += system_response[i_meas][i_bin];
        }
        normalized_vector.push_back(temp_value);
    }

    return normalized_vector;
}


//==================================================================================================
// Return a poisson_distribution object, d that can be sampled from
//  - accept a parameter 'lamda', that is used as the mean & std. dev of the poisson distribution
//  - use the 'mrand' generator as the random number generator for the distribution
//==================================================================================================
double poisson(double lambda)
{
    std::poisson_distribution<int> d(lambda); // initialization

    return d(mrand); // sample
}


//==================================================================================================
// Calculate the root-mean-square deviation of a vector of values from a "true" value.
//==================================================================================================
double calculateRMSD(int num_samples, double true_value, std::vector<double> &sample_vector) {
    double sum_sq_diff = 0;

    // Sum the square difference of poisson sampled values from the true value
    for (int i_samp = 0; i_samp < num_samples; i_samp++) {
        sum_sq_diff += ((true_value - sample_vector[i_samp])*(true_value - sample_vector[i_samp]));
    }
    double avg_sq_diff = sum_sq_diff/num_samples;
    double rms_diff = sqrt(avg_sq_diff);

    return rms_diff;
}


//==================================================================================================
// Calculate the root-mean-square deviation of sampled vectors from a "true" vector.
//==================================================================================================
int calculateRMSD_vector(int num_samples, std::vector<double> &true_vector, std::vector<std::vector<double>> &sampled_vectors, std::vector<double> &rms_differences) {
    int true_vector_size = true_vector.size();

    // Loop over each element in the true vector
    for (int i = 0; i < true_vector_size; i++) {
        double sum_sq_diff = 0;
        // Sum the square difference of poisson sampled values from the true value
        for (int i_samp = 0; i_samp < num_samples; i_samp++) {
            sum_sq_diff += ((true_vector[i] - sampled_vectors[i_samp][i])*(true_vector[i] - sampled_vectors[i_samp][i]));
        }
        double avg_sq_diff = sum_sq_diff/num_samples;
        double rms_diff = sqrt(avg_sq_diff);
        rms_differences.push_back(rms_diff);
    }
    return 1;
}


//==================================================================================================
// Calculate the total ambient dose equivalent rate associated with the provided spectrum using
// weighting factors (binned by energy) provided in ICRP 74.
//==================================================================================================
double calculateDose(int num_bins, std::vector<double> &spectrum, std::vector<double> &icrp_factors) {
    double ambient_dose_eq = 0;
    int s_to_hr = 3600;
    double msv_to_psv = 1e-9;

    // Sum the dose contributions from each energy bin
    for (int i_bins=0; i_bins < num_bins; i_bins++) {
        ambient_dose_eq += spectrum[i_bins]*icrp_factors[i_bins];
    }

    // Convert from [pSv/s] to [mSv/hr]
    ambient_dose_eq *= (s_to_hr)*(msv_to_psv);

    return ambient_dose_eq;
}


//==================================================================================================
// Calculate the total measured charge associated with a series of NNS measurements
//==================================================================================================
double calculateTotalCharge(int num_measurements, std::vector<double> measurements_nc) {
    double total_charge = 0;

    for (int i_meas=0; i_meas < num_measurements; i_meas++) {
        total_charge += measurements_nc[i_meas];
    }

    return total_charge;
}


//==================================================================================================
// Calculate the total neutron flux of a neutron flux spectrum
//==================================================================================================
double calculateTotalFlux(int num_bins, std::vector<double> &spectrum) {
    double total_flux = 0;

    for (int i_bin=0; i_bin < num_bins; i_bin++) {
        total_flux += spectrum[i_bin];
    }

    return total_flux;
}


//==================================================================================================
// Calculate the average neutron energy of a neutron flux spectrum.
// - Normalize the flux spectrum by the total flux: result is relative contribution of each energy
//      bin to the total flux
// - Multiply the energy of each bin by its relative contribution & sum
//==================================================================================================
double calculateAverageEnergy(int num_bins, std::vector<double> &spectrum, std::vector<double> &energy_bins) {
    double total_flux = calculateTotalFlux(num_bins,spectrum);
    std::vector<double> normalized_energy = spectrum;
    double avg_energy = 0;

    for (int i_bin=0; i_bin < num_bins; i_bin++) {
        avg_energy += (energy_bins[i_bin] * spectrum[i_bin]/total_flux);
    }

    return avg_energy;
}


//==================================================================================================
// Calculate the uncertainty on a sum of values.
// Implements the standard uncertainty propagation rule for sums.
//==================================================================================================
double calculateSumUncertainty(int num_values, std::vector<double> &value_uncertainties) {
    double sum_uncertainty = 0;

    for (int i=0; i < num_values; i++) {
        sum_uncertainty += pow(value_uncertainties[i],2);
    }

    return sqrt(sum_uncertainty);
}


//==================================================================================================
// Calculate the uncertainty on the average energy
// Implements standard uncertainty propagation rules for products & sums.
//==================================================================================================
double calculateEnergyUncertainty(int num_bins, std::vector<double> energy_bins, std::vector<double> spectrum, std::vector<double> spectrum_uncertainty, double total_flux, double total_flux_uncertainty) {
    double energy_uncertainty = 0;

    for (int i_bin = 0; i_bin < num_bins; i_bin++) {
        double temp1 = energy_bins[i_bin] * spectrum[i_bin] / total_flux;
        double temp2 = sqrt(pow(spectrum_uncertainty[i_bin]/spectrum[i_bin],2)+pow(total_flux_uncertainty/total_flux,2));
        double temp3 = temp1*temp2;
        energy_uncertainty += pow(temp3,2);
    }

    energy_uncertainty = sqrt(energy_uncertainty);

    return energy_uncertainty;
}


//==================================================================================================
// Calculate the neutron source strength (shielding quanitiy of interest) of a neutron flux spectrum
// Neutron source strength = # neutrons emitted from head per Gy of photon dose delivered to 
//  isocentre

// - Empirical formula for total neutron fluence is provided in NCRP 151 pg. 42 (Eq 2.16) as a
//      function of neutron source strength. Can rearrage for source strength.
//
// Given a neutron flux spectrum:
// - Convert total flux to # neutrons per Gy per cm^2 (using dose, doserate, time, and MU->cGy calibration)
// - Divide by empirical relationship to get neutron source strength
//==================================================================================================
double calculateSourceStrength(int num_bins, std::vector<double> &spectrum, int duration, double dose_mu) {
    double total_flux = calculateTotalFlux(num_bins,spectrum);
    // Fraction of neutrons that penetrate head shielding
    double transmission_factor = 0.93; // Average of 1 for Pb and 0.85 for W
    // Surface area of treatment room [cm^2]
    double room_surface_area = 2353374.529; // For MGH.. But strength does not vary much with size
    // div by 6 = 392229 cm^2 per wall
    // l = w = 626 cm = 6.26 m (room dimension)

    // Distance from from source (Bremsstrahlung target) to point where flux was evaluated [cm]
    double distance = 100;
    // Factor to convert MU to Gy
    double mu_to_gy = 100;

    // Convert total flux to total fluence per Gy photon dose at isocenter 
    double fluence_total = total_flux * duration / dose_mu * mu_to_gy;

    // Factors in the empirical formula
    double fluence_direct_factor = transmission_factor/(4 * M_PI * pow(distance,2));
    double fluence_scatter_factor = 5.4 * transmission_factor / room_surface_area;
    double fluence_thermal_factor = 1.26 / room_surface_area;

    // Calculate source strength
    double source_strength = fluence_total / (fluence_direct_factor + fluence_scatter_factor + fluence_thermal_factor);

    return source_strength;
}


//==================================================================================================
// Accept a series of measurements and an estimated input spectrum and perform the MLEM algorithm
// until the true spectrum has been unfolded. Use the provided target error (error) and the maximum
// number of MLEM iterations (cutoff) to determine when to cease execution of the algorithm. Note
// that spectrum is updated as the algorithm progresses (passed by reference). Similarly for 
// mlem_ratio
//==================================================================================================
int runMLEM(int cutoff, double error, int num_measurements, int num_bins, std::vector<double> &measurements, std::vector<double> &spectrum, std::vector<std::vector<double>> &nns_response, std::vector<double> &normalized_response, std::vector<double> &mlem_ratio) {
    int mlem_index; // index of MLEM iteration

    for (mlem_index = 0; mlem_index < cutoff; mlem_index++) {
        mlem_ratio.clear(); // wipe previous ratios for each iteration

        // vector that stores the MLEM-estimated data to be compared with measured data
        std::vector<double> mlem_estimate;

        // Apply system matrix, the nns_response, to current spectral estimate to get MLEM-estimated
        // data. Save results in mlem_estimate
        // Units: mlem_estimate [cps] = nns_response [cm^2] x spectru  [cps / cm^2]
        for(int i_meas = 0; i_meas < num_measurements; i_meas++)
        {
            double temp_value = 0;
            for(int i_bin = 0; i_bin < num_bins; i_bin++)
            {
                temp_value += nns_response[i_meas][i_bin]*spectrum[i_bin];
            }
            mlem_estimate.push_back(temp_value);
        }

        // Calculate ratio between each measured data point and corresponding MLEM-estimated data point
        for(int i_meas = 0; i_meas < num_measurements; i_meas++)
        {
            mlem_ratio.push_back(measurements[i_meas]/mlem_estimate[i_meas]);
        }

        // matrix that stores the correction factor to be applied to each MLEM-estimated spectral value
        std::vector<double> mlem_correction;

        // Create the correction factors to be applied to MLEM-estimated spectral values:
        //  - multiply transpose system matrix by ratio values
        for(int i_bin = 0; i_bin < num_bins; i_bin++)
        {
            double temp_value = 0;
            for(int i_meas = 0; i_meas < num_measurements; i_meas++)
            {
                temp_value += nns_response[i_meas][i_bin]*mlem_ratio[i_meas];
            }
            mlem_correction.push_back(temp_value);
        }

        // Apply correction factors and normalization to get new spectral estimate
        for(int i_bin=0; i_bin < num_bins; i_bin++)
        {
            spectrum[i_bin] = (spectrum[i_bin]*mlem_correction[i_bin]/normalized_response[i_bin]);
        }

        // End MLEM iterations if ratio between measured and MLEM-estimated data points is within
        // tolerace specified by 'error'
        bool continue_mlem = false;
        for (int i_meas=0; i_meas < num_measurements; i_meas++) {
            if (mlem_ratio[i_meas] >= (1+error) || mlem_ratio[i_meas] <= (1-error)) {
                continue_mlem = true;
                break;
            }   
        }
        if (!continue_mlem) {
            break;
        }
    }

    return mlem_index;
}



//==================================================================================================
// Accept a series of measurements and an estimated input spectrum and perform the MLEM algorithm
// until the true spectrum has been unfolded. Use the provided target error (error) and the maximum
// number of MLEM iterations (cutoff) to determine when to cease execution of the algorithm. Note
// that spectrum is updated as the algorithm progresses (passed by reference). Similarly for 
// mlem_ratio
//==================================================================================================
int runMAP(std::vector<double> &energy_correction, double beta, int cutoff, double error, int num_measurements, int num_bins, std::vector<double> &measurements, std::vector<double> &spectrum, std::vector<std::vector<double>> &nns_response, std::vector<double> &normalized_response, std::vector<double> &mlem_ratio) {
    int mlem_index; // index of MLEM iteration

    for (mlem_index = 0; mlem_index < cutoff; mlem_index++) {
        mlem_ratio.clear(); // wipe previous ratios for each iteration

        // vector that stores the MLEM-estimated data to be compared with measured data
        std::vector<double> mlem_estimate;

        // Apply system matrix, the nns_response, to current spectral estimate to get MLEM-estimated
        // data. Save results in mlem_estimate
        // Units: mlem_estimate [cps] = nns_response [cm^2] x spectru  [cps / cm^2]
        for(int i_meas = 0; i_meas < num_measurements; i_meas++)
        {
            double temp_value = 0;
            for(int i_bin = 0; i_bin < num_bins; i_bin++)
            {
                temp_value += nns_response[i_meas][i_bin]*spectrum[i_bin];
            }
            mlem_estimate.push_back(temp_value);
        }

        // Calculate ratio between each measured data point and corresponding MLEM-estimated data point
        for(int i_meas = 0; i_meas < num_measurements; i_meas++)
        {
            mlem_ratio.push_back(measurements[i_meas]/mlem_estimate[i_meas]);
        }

        // matrix that stores the correction factor to be applied to each MLEM-estimated spectral value
        std::vector<double> mlem_correction;

        // Create the correction factors to be applied to MLEM-estimated spectral values:
        //  - multiply transpose system matrix by ratio values
        for(int i_bin = 0; i_bin < num_bins; i_bin++)
        {
            double temp_value = 0;
            for(int i_meas = 0; i_meas < num_measurements; i_meas++)
            {
                temp_value += nns_response[i_meas][i_bin]*mlem_ratio[i_meas];
            }
            mlem_correction.push_back(temp_value);
        }

        // Create the MAP energy correction factors to be incorporated in the normalization
        // std::vector<double> energy_correction;
        energy_correction.clear();

        energy_correction.push_back(beta*pow(spectrum[0]-spectrum[1],2));
        for (int i_bin=1; i_bin < num_bins-1; i_bin++)
        {
            double temp_value = 0;
            temp_value = beta * (pow(spectrum[i_bin]-spectrum[i_bin-1],2)+pow(spectrum[i_bin]-spectrum[i_bin+1],2));          
            energy_correction.push_back(temp_value);
        }
        energy_correction.push_back(beta*pow(spectrum[num_bins-1]-spectrum[num_bins-2],2));

        // Apply correction factors and normalization to get new spectral estimate
        for(int i_bin=0; i_bin < num_bins; i_bin++)
        {
            spectrum[i_bin] = spectrum[i_bin]*mlem_correction[i_bin]/(normalized_response[i_bin]+energy_correction[i_bin]);
        }

        // End MLEM iterations if ratio between measured and MLEM-estimated data points is within
        // tolerace specified by 'error'
        bool continue_mlem = false;
        for (int i_meas=0; i_meas < num_measurements; i_meas++) {
            if (mlem_ratio[i_meas] >= (1+error) || mlem_ratio[i_meas] <= (1-error)) {
                continue_mlem = true;
                break;
            }   
        }
        if (!continue_mlem) {
            break;
        }
    }

    return mlem_index;
}
