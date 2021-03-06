// ArrayGeometry.C
// Checks the excitation of each state and sees where it starts getting clipped
// ============================================================================================= //
// Patrick MacGregor
// Nuclear Physics Research Group
// School of Physics and Astronomy
// The University of Manchester
// ============================================================================================= //
#include "AG_physical_constants.h"
#include "AG_constants.h"
#include "AG_functions.h"
#include "AG_style.h"

#include <TCanvas.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TBox.h>
#include <TEllipse.h>
#include <TLine.h>
#include <TRandom.h>

#include <iostream>
#include <fstream>

void ArrayGeometryEX( Double_t ex = EX ){
	// Set style
	CreateStyle( ptm_style );
	ptm_style->cd();
	if ( BATCH_MODE ){ gROOT->SetBatch(kTRUE); }

	// DRAW STUFF ------------------------------------------------------------------------------ //
	TCanvas* c0 = new TCanvas( "c_array", "Array Trajectories", CANVAS_WIDTH, CANVAS_HEIGHT );
	
	// Draw frame
	TH1F* frame = c0->DrawFrame( X1, Y1, X2, Y2 );
	frame->SetTitle( ";z / cm; r / cm" );

	// Draw target
	TBox* b_target = new TBox( 0, -2, 0.5, 2 );
	b_target->SetFillColor(kBlack);
	b_target->Draw("SAME");

	// Draw PCB
	TBox* b_pcb = new TBox( - Si_centroids[POSITION][5] + LAST_CENTROID_TO_PCB - PCB_LENGTH, -PCB_WIDTH/2, - Si_centroids[POSITION][5] + LAST_CENTROID_TO_PCB, PCB_WIDTH/2 );
	b_pcb->SetFillColor(pcb_green_i);
	b_pcb->Draw("SAME");

	// Draw 4-jaw slits
	TBox* b_four_jaw = new TBox( b_pcb->GetX2(), -ARR_DIAM/2, b_pcb->GetX2() + SLIT_LENGTH, ARR_DIAM/2 );
	b_four_jaw->SetFillColor( fj_red_i );
	b_four_jaw->Draw("SAME");

	// Draw Si strips
	TBox* b_Si[6];
	for ( Int_t a = 0; a < 6; a++ ){
		b_Si[a] = new TBox( -SI_WIDTH/2 - Si_centroids[POSITION][a], -SI_HEIGHT/2, SI_WIDTH/2 - Si_centroids[POSITION][a], SI_HEIGHT/2 );
		b_Si[a]->SetFillColor(si_strip_i);
		b_Si[a]->Draw("SAME");
	}

	// Calculate the required z
	Double_t z_fj = b_pcb->GetX2() + SLIT_LENGTH;

	
	// CALCULATIONS ---------------------------------------------------------------------------- //
	// Declare variables
	Double_t* theta_cm = new Double_t[NUM_THETA];
	Double_t* theta_lab = new Double_t[NUM_THETA];

	// Store trajectories in x[phi coordinate][z coordinate]
	Double_t** x = new Double_t*[NUM_EVENTS_PER_THETA];
	Double_t** y = new Double_t*[NUM_EVENTS_PER_THETA];
	Double_t** z = new Double_t*[NUM_EVENTS_PER_THETA];
	for ( Int_t i = 0; i < NUM_EVENTS_PER_THETA; i++ ){
		x[i] = new Double_t[NUM_Z];
		y[i] = new Double_t[NUM_Z];
		z[i] = new Double_t[NUM_Z];
	}
	
	Double_t** d_phi = new Double_t*[NUM_THETA];
	Double_t** r = new Double_t*[NUM_THETA];
	for ( Int_t i = 0; i < NUM_THETA; i++ ){
		d_phi[i] = new Double_t[3];
		r[i] = new Double_t[NUM_Z];
	}

	Double_t solid_angle[NUM_THETA];

	// Calculate angle-independent terms
	Double_t T_cm_f = T_cm_i + Q - ex;							// Final energy in CM [MeV]
	Double_t T_cm_3 = mass[3]*T_cm_f/( mass[2] + mass[3] );		// Final energy of ejectile in CM [MeV]
	Double_t v3 = TMath::Sqrt( 2*T_cm_3/mass[2] );				// Final velocity of ejectile in CM [c]
	
	// Declare doubles for angle-specific kinematic terms
	Double_t T_lab_3, v_para, v_perp, theta_var, phi, beam_offset_r, beam_offset_theta;
	TRandom rand_phi, rand_beam_offset_r, rand_beam_offset_theta;

	// Calculate contributions to solid angle (roughly)
	Double_t phi_cont = 360.0*1.0/NUM_EVENTS_PER_THETA;
	
	// Declare TGraph's to hold trajectory information
	TGraph *g_traj[NUM_THETA];				// For radii at a given theta
	for ( Int_t i = 0; i < NUM_THETA; i++ ){ g_traj[i] = NULL; }
	TGraph *g_traj_phi[NUM_EVENTS_PER_THETA];			// For trajectories in phi
	for ( Int_t i = 0; i < NUM_EVENTS_PER_THETA; i++ ){ g_traj_phi[i] = NULL; }

	// Define a TEllipse to hold the start points of each trajectory
	TEllipse *beam_start[NUM_EVENTS_PER_THETA];
	for ( Int_t i = 0; i < NUM_EVENTS_PER_THETA; i++ ){ beam_start[i] = NULL; }

	// Open log file
	std::ofstream log_file;
	log_file.open( Form( "output-data/EX_%5.4f-POS_%i.dat", ex, POSITION ), std::ofstream::out );

	// Percentage progress holder
	Double_t prog = 0.0;
	
	// Loop over centre-of-mass angle (theta)
	for ( Int_t i = 0; i < NUM_THETA; i++ ){
		// Calculate angle-dependent kinematic terms
		theta_cm[i] = ( i*theta_spacing + theta_lb );
		T_lab_3 = T_cm_3 + 0.5*mass[2]*V_cm*V_cm + mass[2]*v3*V_cm*TMath::Cos( theta_cm[i]*TMath::DegToRad() );	// [MeV]
		theta_var = 180 - theta_cm[i];									// The true CM angle
		v_para = V_cm + v3*TMath::Cos( theta_var*TMath::DegToRad() );												// [c]
		v_perp = v3*TMath::Sin( theta_var*TMath::DegToRad() );														// [c]
		theta_lab[i] = TMath::ASin( v_perp/TMath::Sqrt( v_para*v_para + v_perp*v_perp ) )*TMath::RadToDeg();		

		// Define x and y success bools, as well as a stop flag for phi
		Bool_t x_success, y_success, x_failure, y_failure, z_failure;
		Bool_t phi_stop = 0;
		Int_t traj_colour[NUM_EVENTS_PER_THETA];
		for ( Int_t j = 0; j < NUM_EVENTS_PER_THETA; j++ ){ traj_colour[j] = (Int_t)kRed; }

		// Initialise phi solid angle array parameters
		d_phi[i][0] = 0.0;
		d_phi[i][1] = 0.0;
		d_phi[i][2] = 0.0;
	

		// Calculate the trajectory --> loop over phi
		for ( Int_t j = 0; j < NUM_EVENTS_PER_THETA; j++ ){
		// Calculate trajectories for a given theta
			phi = rand_phi.Uniform(0,360);			// This is initial phi angle
			beam_offset_r = rand_beam_offset_r.Gaus(0, BEAM_FWHM*FWHMToSigma() );
			beam_offset_theta = rand_beam_offset_theta.Uniform(0,180);
			phi_stop = 0;
			
			// Loop over z
			for ( Int_t k = 0; k < NUM_Z; k++ ){
				// Calculate x,y,z
				if ( phi_stop == 0 ){
					z[j][k] = -1.0*z_spacing*k;
					x[j][k] = v_perp*UC_CToMS()*UC_MToCM()*( TMath::Cos( phi*TMath::DegToRad() ) - TMath::Cos( phi*TMath::DegToRad() + cyclotron_freq*z[j][k]/( v_para*UC_CToMS()*UC_MToCM() ) ) )/cyclotron_freq + beam_offset_r*TMath::Cos( beam_offset_theta*TMath::DegToRad() ) + BEAM_SPOT_OFF_X;
					y[j][k] = v_perp*UC_CToMS()*UC_MToCM()*( -TMath::Sin( phi*TMath::DegToRad() ) + TMath::Sin( phi*TMath::DegToRad() + cyclotron_freq*z[j][k]/( v_para*UC_CToMS()*UC_MToCM() ) ) )/cyclotron_freq + beam_offset_r*TMath::Sin( beam_offset_theta*TMath::DegToRad() ) + BEAM_SPOT_OFF_Y;
				}
				else if ( phi_stop == 1 && k > 0 ){
					x[j][k] = x[j][k-1];
					y[j][k] = y[j][k-1];
					z[j][k] = z[j][k-1];
				}

				// Calculate success/failures
				if ( phi_stop == 0 ){
					x_success = ( TMath::Abs( x[j][k] ) <= ARR_DIAM/2 && TMath::Abs( y[j][k] ) < SI_HEIGHT/2 && z[j][k] <= z_fj );
					y_success = ( TMath::Abs( y[j][k] ) <= ARR_DIAM/2 && TMath::Abs( x[j][k] ) < SI_HEIGHT/2 && z[j][k] <= z_fj );
					x_failure = ( TMath::Abs( x[j][k] ) <= ARR_DIAM/2 && TMath::Abs( y[j][k] ) > SI_HEIGHT/2 && TMath::Abs( y[j][k] ) <= ARR_DIAM/2 && z[j][k] <= z_fj );
					y_failure = ( TMath::Abs( y[j][k] ) <= ARR_DIAM/2 && TMath::Abs( x[j][k] ) > SI_HEIGHT/2 && TMath::Abs( x[j][k] ) <= ARR_DIAM/2 && z[j][k] <= z_fj );
					z_failure = ( TMath::Abs( x[j][k] ) <= ARR_DIAM/2 && TMath::Abs( y[j][k] ) <= ARR_DIAM/2 && z[j][k] <= z_fj + 0.5*z_spacing && z[j][k] >= z_fj - 0.5*z_spacing );
				

					if ( ( x_success || y_success ) ){
						phi_stop = 1;
						traj_colour[j] = (Int_t)kGreen;
						d_phi[i][0] += phi_cont;
					}
					else if ( ( x_failure || y_failure ) ){
						phi_stop = 1;
						traj_colour[j] = (Int_t)kOrange;
						d_phi[i][1] += phi_cont;
					}
					else if ( z_failure ){
						phi_stop = 1;
						traj_colour[j] = (Int_t)kRed;
						d_phi[i][2] += phi_cont;
					}

				}

				// Calculate the radius and print useful values to file
				if ( j == 0 ){
					r[i][k] = y[j][k];
				}

			}	// Loop over z (k)
			
			// Draw successful trajectories in phi
			if ( theta_cm[i] == theta_lb ){
				g_traj_phi[j] = new TGraph( NUM_Z, x[j], y[j] );
				g_traj_phi[j]->SetLineColor( traj_colour[j] );

				beam_start[j] = new TEllipse( x[j][0], y[j][0], 0.01);
				beam_start[j]->SetFillColor( traj_colour[j] );
			}
			// Draw trajectories in theta for a random phi
			if ( j == 0 && i % TMath::Max( (Int_t)TMath::Floor( (Double_t)NUM_THETA/20.0 ), 1 ) == 0 ){
				g_traj[i] = new TGraph( NUM_Z, z[j], r[i] );
				g_traj[i]->SetLineColor( traj_colour[j] );
				g_traj[i]->Draw();
			}
			
		}	// Loop over phi + radius (j)
		

		// Point to solid angle (for graphing purposes)
		solid_angle[i] = d_phi[i][0];

		// Print to file
		if ( i == 0 ){
			log_file << "    THETA_CM   THETA_LAB     X_FINAL     Y_FINAL     Z_FINAL        SA_1        SA_2        SA_3\n";
		}
		
		// Standard line to file
		log_file << std::fixed << std::setw(12) << std::setprecision(4) << theta_cm[i] << std::setw(12) << std::setprecision(4) << theta_lab[i] << std::setw(12) << std::setprecision(4) << x[0][NUM_Z-1] << std::setw(12) << std::setprecision(4) << y[0][NUM_Z-1] << std::setw(12) << std::setprecision(4) << z[0][NUM_Z-1] << std::setw(12) << std::setprecision(4) << d_phi[i][0] << std::setw(12) << std::setprecision(4) << d_phi[i][1] << std::setw(12) << std::setprecision(4) << d_phi[i][2] << "\n";
		// Output percentage in terminal
		prog = 100.0*(Double_t)i/(Double_t)NUM_THETA;
		std::cout << std::fixed << std::setprecision(2) << prog << "% complete\r" << std::flush;
		

	}	// Loop over theta (i)
	
	// CALCULATE THE AVERAGE SOLID ANGLE
	Double_t solid_angle_mav[NUM_THETA];
	Int_t half_window_size = 4; // Multiple by 2*i + 1 to get window width
	
	for ( Int_t i = 0; i < NUM_THETA; i++ ){
		if ( i == half_window_size ){
			Double_t sum = 0;
			for ( Int_t j = 0; j < 2*half_window_size + 1; j++ ){
				sum += solid_angle[i + j - half_window_size];
			}
			solid_angle_mav[i] = sum/(2*half_window_size + 1);
		}
		else if ( i < NUM_THETA - half_window_size && i > half_window_size ){
			solid_angle_mav[i] = solid_angle_mav[i-1] + ( solid_angle[i+half_window_size] - solid_angle[i-1-half_window_size] )/(2*half_window_size + 1);
		}
		else{
			solid_angle_mav[i] = 0.0;
		}
	}


	// DRAW THE ARRAY HEAD ON
	TCanvas* c1 = new TCanvas( "c_clip", "Clipping on the array", CANVAS_HEIGHT, CANVAS_HEIGHT );
	c1->cd();
	c1->DrawFrame( -10, -10, 10, 10 );

	// Draw the four jaws and the inside of the array
	TBox* b_fj_head = new TBox( -ARR_DIAM/2, -ARR_DIAM/2, ARR_DIAM/2, ARR_DIAM/2 );
	b_fj_head->SetFillColor( fj_red_i );
	b_fj_head->Draw("SAME");

	TBox* b_fj_inside = new TBox( -ARR_IN_DIAM/2, -ARR_IN_DIAM/2, ARR_IN_DIAM/2, ARR_IN_DIAM/2 );
	b_fj_inside->SetFillColor( kBlack );
	b_fj_inside->Draw("SAME");

	// Draw Si detectors
	TBox *b_si_headon[4];
	b_si_headon[0] = new TBox( -SI_HEIGHT/2, -ARR_DIAM/2, SI_HEIGHT/2, -0.9*ARR_DIAM/2 );
	b_si_headon[1] = new TBox( -SI_HEIGHT/2, ARR_DIAM/2, SI_HEIGHT/2, 0.9*ARR_DIAM/2 );
	b_si_headon[2] = new TBox( -ARR_DIAM/2, -SI_HEIGHT/2, -0.9*ARR_DIAM/2, SI_HEIGHT/2 );
	b_si_headon[3] = new TBox( ARR_DIAM/2, -SI_HEIGHT/2, 0.9*ARR_DIAM/2, SI_HEIGHT/2 );
	for ( Int_t j = 0; j < 4; j++ ){
		b_si_headon[j]->SetFillColor(kBlack);
		b_si_headon[j]->Draw("SAME");
	}
	
	// Draw trajectories
	for ( Int_t j = 0; j < NUM_EVENTS_PER_THETA; j++ ){
		if ( j % TMath::Max( (Int_t)TMath::Floor( (Double_t)NUM_EVENTS_PER_THETA/120.0 ), 1 ) == 0 ){
			g_traj_phi[j]->Draw("C SAME");
			beam_start[j]->Draw("SAME");
		}
	}

	// Draw the phi fraction
	TCanvas* c2 = new TCanvas( "c_phi_frac", "Fraction of phi obstructed", CANVAS_WIDTH, CANVAS_HEIGHT );
	c2->cd();
	c2->DrawFrame( TMath::Max( 0.0, theta_lb - theta_spacing ), 0, theta_ub + theta_spacing, 360 );
	TGraph* g_phi_frac = new TGraph( NUM_THETA, theta_cm, solid_angle );
	TGraph* g_phi_frac_mav = new TGraph( NUM_THETA, theta_cm, solid_angle_mav );
	
	g_phi_frac->SetTitle("");
	g_phi_frac->GetXaxis()->SetTitle( "#theta_{cm} / ^{#circ}" );
	g_phi_frac->GetYaxis()->SetTitle( "#Delta#phi" );
	g_phi_frac->SetMarkerStyle(20);
	g_phi_frac->SetMarkerSize(0.5);
	g_phi_frac->Draw("LP");

	g_phi_frac_mav->SetMarkerStyle(20);
	g_phi_frac_mav->SetMarkerSize(0.5);
	g_phi_frac_mav->SetLineColor(kBlue);
	g_phi_frac_mav->SetMarkerColor(kBlue);
	g_phi_frac_mav->Draw("LP SAME");

	TLine *l_solid_angle = new TLine( c2->GetUxmin(), 8*TMath::ATan( 9.0/23.0 )*TMath::RadToDeg(), c2->GetUxmax(), 8*TMath::ATan( 9.0/23.0 )*TMath::RadToDeg() );
	l_solid_angle->SetLineStyle(2);
	l_solid_angle->SetLineColor(kRed);
	l_solid_angle->SetLineWidth(2);
	l_solid_angle->Draw("SAME");
	
	// Print the canvases
	c0->Print( Form( "output-data/EX_%5.4f-POS_%i-array_side.svg", ex, POSITION ) );
	c1->Print( Form( "output-data/EX_%5.4f-POS_%i-array_head_on.svg", ex, POSITION ) );
	c2->Print( Form( "output-data/EX_%5.4f-POS_%i-phi_frac.svg", ex, POSITION ) );

	// Close the log file
	log_file.close();

	if ( !BATCH_MODE ){ gPad->WaitPrimitive("TPave"); }
	std::cout << "Deleting variables...\n";

	// Clear the memory
	// Arrays
	delete[] theta_cm;
	delete[] theta_lab;
	delete[] x;
	delete[] y;
	delete[] z;
	delete[] d_phi;
	delete[] r;

	// TCanvases
	if ( c0->IsOnHeap() ){ delete c0; }
	if ( c1->IsOnHeap() ){ delete c1; }
	if ( c2->IsOnHeap() ){ delete c2; }

	// TBoxes
	if ( b_target->IsOnHeap() ){ b_target->Delete(); }
	if ( b_pcb->IsOnHeap() ){ b_pcb->Delete(); }
	if ( b_four_jaw->IsOnHeap() ){ b_four_jaw->Delete(); }
	for ( Int_t i = 0; i < 6; i++ ){
		if ( b_Si[i]->IsOnHeap() ){ b_Si[i]->Delete(); }
	}
	if ( b_fj_head->IsOnHeap() ){ b_fj_head->Delete(); }
	if ( b_fj_inside->IsOnHeap() ){ b_fj_inside->Delete(); }
	for ( Int_t i = 0; i < 4; i++ ){
		if ( b_si_headon[i]->IsOnHeap() ){ b_si_headon[i]->Delete(); }
	}

	// TGraphs
	for ( Int_t i = 0; i < NUM_EVENTS_PER_THETA; i++ ){
		if ( g_traj_phi[i] != NULL ){ if ( g_traj_phi[i]->IsOnHeap() ){ g_traj_phi[i]->Delete(); } }
	}
	for ( Int_t i = 0; i < NUM_THETA; i++ ){
		if ( g_traj[i] != NULL ){ if ( g_traj[i]->IsOnHeap() ){ g_traj[i]->Delete(); } }
	}
	if ( g_phi_frac->IsOnHeap() ){ g_phi_frac->Delete(); }

	// TLines
	if ( l_solid_angle->IsOnHeap() ){ l_solid_angle->Delete(); }

	// TEllipses
	for ( Int_t i = 0; i < NUM_EVENTS_PER_THETA; i++ ){
		if ( beam_start[i]->IsOnHeap() ){ beam_start[i]->Delete(); }
	}

}




void ArrayGeometry(){
	const Int_t NUM_STATES = 10;
	const Bool_t ALL = 0;
	Double_t STATES[NUM_STATES] = {
		0.00000,
		0.05460,
		1.09109,
		1.42959,
		2.24696,
		2.48435,
		2.95315,
		3.22944,
		3.97709,
		4.31613
	};

	if ( ALL == 1 ){
		for ( Int_t i = 0; i < NUM_STATES; i++ ){
			ArrayGeometryEX( STATES[i] );
			gROOT->ls();
		}
	}
	else{
		ArrayGeometryEX( STATES[0] );
	}

}



/* *** TODO ***
 * Residuals for phi_frac plot
 * Error bars for phi_frac plot?
 * Reduce memory usage for high-resolution - what can I afford to not store?
 * Force j=0 -> phi=0 for plots
 * Save in ROOT files (in tree?) for ease of manipulation
 * Axis labels on all of the graphs
 * Fonts on all aspects of graphs
 * Change phi_frac graph to have only black points
*/



















