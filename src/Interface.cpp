

#include "Interface.h"
#include <sstream>

Interface::Interface()
{
}

Interface::Interface(Cell* negCell, Cell* posCell, int axis, float2 normal, FluidParameter fluidParam, BoundaryCondition* BC)
{
	this->negCell = negCell;
	this->posCell = posCell;
    
    // links to interfaces
    //    ---------------------
    //    |    3    |    3    |
    //    | 0     2 | 0     2 |
    //    |    1    |    1    |
    //    ---------------------
    //     neg Cell   pos Cell
    //
    //    -----------
    //    |    3    |
    //    | 0     2 |   pos Cell
    //    |    1    |
    //    -----------
    //    |    3    |
    //    | 0     2 |   neg Cell
    //    |    1    |
    //    -----------

	if(this->negCell != NULL) this->negCell->addInterface(this,axis+2);
	if(this->posCell != NULL) this->posCell->addInterface(this,axis+0);

    this->normal = normal;
    this->axis = axis;

    this->fluidParam = fluidParam;

    this->BoundaryConditionPointer = BC;
}

Interface::~Interface()
{
}

void Interface::computeFlux(double dt)
{


    const int NUMBER_OF_MOMENTS = 7;

    double prim[4];
    double normalGradCons[4];
    double tangentialGradCons[4];
    double timeGrad[4];

    double a[4];
    double b[4];
    double A[4];

    double MomentU[NUMBER_OF_MOMENTS];
    double MomentV[NUMBER_OF_MOMENTS];
    double MomentXi[NUMBER_OF_MOMENTS];

    // ========================================================================
    
    if (this->BoundaryConditionPointer != NULL)
    {
        double rhoV;
        double dx;
        double dy;
        double sign;

        if (this->posCell != NULL)
        {
            if (this->axis == 0)
            {
                rhoV = this->posCell->getCons().rhoV;
                dx   = this->posCell->getDx().x;
                dy   = this->posCell->getDx().y;
            }                
            else             
            {                
                rhoV = this->posCell->getCons().rhoU;
                dx   = this->posCell->getDx().y;
                dy   = this->posCell->getDx().x;
            }
            sign = -1.0;
        }
        else if (this->negCell != NULL)
        {
            if (this->axis == 0)
            {
                rhoV = this->negCell->getCons().rhoV;
                dx   = this->negCell->getDx().x;
                dy   = this->negCell->getDx().y;
            }
            else
            {
                rhoV = this->negCell->getCons().rhoU;
                dx   = this->negCell->getDx().y;
                dy   = this->negCell->getDx().x;
            }
            sign = 1.0;
        }

        this->Flux[0] = 0.0;
        this->Flux[1] = 0.0;
        this->Flux[2] = sign * 2.0 * this->fluidParam.nu * rhoV / dx * dt;
        this->Flux[3] = 0.0;

        if (this->axis == 1)
            this->rotate(this->Flux);

        return;
    }

    // ========================================================================

    // compute the length of the interface
    double dy = this->posCell->getDx().x * normal.y
              + this->posCell->getDx().y * normal.x;

    this->interpolatePrim(prim);
    this->differentiateCons(normalGradCons, tangentialGradCons, prim);

    // Formular as in the Rayleigh-Bernard-Paper (Xu, Lui, 1999)
    double tau = 2.0*prim[3] * this->fluidParam.nu;

    // time integration Coefficients
    double timeCoefficients[3] = { dt, -tau*dt, 0.5*dt*dt - tau*dt };

    // in case of horizontal interface (G interface), swap velocity directions
    if (this->axis == 1)
    {
        this->rotate(prim);
        this->rotate(normalGradCons);
        this->rotate(tangentialGradCons);
    }

    // spacial micro slopes a = a1 + a2 u + a3 v
    //                      b = b1 + b2 u + b3 v

    this->computeMicroSlope(prim, normalGradCons,     a);
    this->computeMicroSlope(prim, tangentialGradCons, b);

    // temporal micro slopes A = A1 + A2 u + A3 v

    this->computeMoments(prim, MomentU, MomentV, MomentXi, NUMBER_OF_MOMENTS);

    this->computeTimeDerivative(prim, MomentU, MomentV, MomentXi, a, b, timeGrad);

    this->computeMicroSlope(prim, timeGrad, A);

    // compute mass and momentum fluxes

    this->assembleFlux(MomentU, MomentV, MomentXi, a, b, A, timeCoefficients, dy, prim);

    // in case of horizontal interface (G interface), swap velocity fluxes
    if (this->axis == 1)
        this->rotate(this->Flux);

}

Cell * Interface::getNeigborCell(Cell * askingCell)
{
    if (posCell == askingCell)
        return negCell;
    else
        return posCell;
}

ConservedVariable Interface::getFlux()
{
    ConservedVariable tmp;
    tmp.rho  = this->Flux[0];
    tmp.rhoU = this->Flux[1];
    tmp.rhoV = this->Flux[2];
    tmp.rhoE = this->Flux[3];
    return tmp;
}

bool Interface::isGhostInterface()
{
    if (this->negCell == NULL || this->posCell == 0)
        return false;
    return this->posCell->isGhostCell() && this->negCell->isGhostCell();
}

string Interface::toString()
{
	ostringstream tmp;
	tmp << "Interface between: \n";
	if (this->negCell != NULL) tmp << this->negCell->toString();
	tmp << "\n";
    if (this->posCell != NULL) tmp << this->posCell->toString();
	tmp << "\n";
    //tmp << this->Flux[0] << " " << this->Flux[1] << " " << this->Flux[2] << " " << this->Flux[3];
    if (this->isGhostInterface())
        tmp << "GhostInterface";
    tmp << "\n";
    tmp << "\n";
	return tmp.str();
}

string Interface::writeCenter()
{
    double x = 0.5 * ( this->posCell->getCenter().x + this->negCell->getCenter().x );
    double y = 0.5 * ( this->posCell->getCenter().y + this->negCell->getCenter().y );

    ostringstream tmp;
    tmp << x << " " << y << " 0.0\n";

    return tmp.str();
}

void Interface::interpolatePrim(double * prim)
{

    prim[0] = 0.5*( this->negCell->getPrim().rho
                  + this->posCell->getPrim().rho );
    prim[1] = 0.5*( this->negCell->getPrim().U
                  + this->posCell->getPrim().U   );
    prim[2] = 0.5*( this->negCell->getPrim().V
                  + this->posCell->getPrim().V   );
    prim[3] = 0.5*( this->negCell->getPrim().L
                  + this->posCell->getPrim().L   );
}

void Interface::differentiateCons(double* normalGradCons, double* tangentialGradCons, double* prim)
{
    // ========================================================================
    // normal direction
    // ========================================================================

    // compute the distance between 
    double dn = ( ( this->posCell->getDx().x + this->negCell->getDx().x ) * normal.x
                + ( this->posCell->getDx().y + this->negCell->getDx().y ) * normal.y ) * 0.5;

    normalGradCons[0] = ( this->posCell->getCons().rho
                        - this->negCell->getCons().rho  ) / dn;

    normalGradCons[1] = ( this->posCell->getCons().rhoU
                        - this->negCell->getCons().rhoU ) / dn;

    normalGradCons[2] = ( this->posCell->getCons().rhoV
                        - this->negCell->getCons().rhoV ) / dn;

    normalGradCons[3] = ( this->posCell->getCons().rhoE
                        - this->negCell->getCons().rhoE ) / dn;

    // ========================================================================
    // tangential direction
    // ========================================================================

    // The tangential derivative is computed by finite difference between the
    // values at the edge of the interface (A, B in fig).
    // These are computed by interpolation.
    //
    //  A = 0.5 (pos + pos pos + neg + neg pos)
    //
    //  ---------------------------------
    //  |               |               |
    //  |    neg pos    |    pos pos    |
    //  |               |               |
    //  --------------- A --------------
    //  |               |               |
    //  |               |               |
    //  |      neg      |      pos      |
    //  |               |               |
    //  |               |               |
    //  --------------- B ---------------
    //  |               |               |
    //  |    neg neg    |    pos neg    |
    //  |               |               |
    //  ---------------------------------

    // get the indieces of the perpendicular interfaces for tangential derivative
    int posIdx;
    int negIdx;
    if (this->axis == 0)
    {
        posIdx = 3;
        negIdx = 1;
    }
    else
    {
        posIdx = 2;
        negIdx = 0;
    }

    // compute the tangential distance (length of the interface)
    double dt = this->posCell->getDx().x * normal.y
              + this->posCell->getDx().y * normal.x;

    // if the cell is located at the boundary, dont use tangential derivatives
    if (   this->posCell->getNeighborCell(posIdx) == NULL
        || this->posCell->getNeighborCell(negIdx) == NULL
        || this->negCell->getNeighborCell(posIdx) == NULL
        || this->negCell->getNeighborCell(negIdx) == NULL )
    {
        tangentialGradCons[0] = 0.0;
        tangentialGradCons[1] = 0.0;
        tangentialGradCons[2] = 0.0;
        tangentialGradCons[3] = 0.0;
    }
    else
    {
        tangentialGradCons[0] = ( ( this->posCell->getNeighborCell(posIdx)->getCons().rho
                                  + this->negCell->getNeighborCell(posIdx)->getCons().rho
                                  + this->posCell->getCons().rho 
                                  + this->negCell->getCons().rho 
                                  ) * 0.25
                                - ( this->posCell->getNeighborCell(negIdx)->getCons().rho
                                  + this->negCell->getNeighborCell(negIdx)->getCons().rho 
                                  + this->posCell->getCons().rho 
                                  + this->negCell->getCons().rho
                                  ) * 0.25
                                ) / dt;

        tangentialGradCons[1] = ( ( this->posCell->getNeighborCell(posIdx)->getCons().rhoU
                                  + this->negCell->getNeighborCell(posIdx)->getCons().rhoU
                                  + this->posCell->getCons().rhoU 
                                  + this->negCell->getCons().rhoU 
                                  ) * 0.25
                                - ( this->posCell->getNeighborCell(negIdx)->getCons().rhoU
                                  + this->negCell->getNeighborCell(negIdx)->getCons().rhoU 
                                  + this->posCell->getCons().rhoU 
                                  + this->negCell->getCons().rhoU 
                                  ) * 0.25
                                ) / dt;

        tangentialGradCons[2] = ( ( this->posCell->getNeighborCell(posIdx)->getCons().rhoV
                                  + this->negCell->getNeighborCell(posIdx)->getCons().rhoV
                                  + this->posCell->getCons().rhoV 
                                  + this->negCell->getCons().rhoV 
                                  ) * 0.25
                                - ( this->posCell->getNeighborCell(negIdx)->getCons().rhoV
                                  + this->negCell->getNeighborCell(negIdx)->getCons().rhoV 
                                  + this->posCell->getCons().rhoV 
                                  + this->negCell->getCons().rhoV 
                                  ) * 0.25
                                ) / dt;

        tangentialGradCons[3] = ( ( this->posCell->getNeighborCell(posIdx)->getCons().rhoE
                                  + this->negCell->getNeighborCell(posIdx)->getCons().rhoE
                                  + this->posCell->getCons().rhoE 
                                  + this->negCell->getCons().rhoE 
                                  ) * 0.25
                                - ( this->posCell->getNeighborCell(negIdx)->getCons().rhoE
                                  + this->negCell->getNeighborCell(negIdx)->getCons().rhoE 
                                  + this->posCell->getCons().rhoE 
                                  + this->negCell->getCons().rhoE 
                                  ) * 0.25
                                ) / dt;
        }
}

void Interface::computeTimeDerivative(double * prim, double * MomentU, double * MomentV, double * MomentXi,
                                      double* a, double* b, double * timeGrad)
{

    timeGrad[0] = a[0] * MomentU[1] * MomentV[0]
                + a[1] * MomentU[2] * MomentV[0]
                + a[2] * MomentU[1] * MomentV[1]
                + a[3] * ( MomentU[3] * MomentV[0] + MomentU[1] * MomentV[2] + MomentU[1] * MomentV[0] * MomentXi[2] )
                + b[0] * MomentU[0] * MomentV[1]
                + b[1] * MomentU[1] * MomentV[1]
                + b[2] * MomentU[0] * MomentV[2]
                + b[3] * ( MomentU[2] * MomentV[1] + MomentU[0] * MomentV[3] + MomentU[0] * MomentV[1] * MomentXi[2] ) ;

    timeGrad[1] = a[0] * MomentU[2] * MomentV[0]
                + a[1] * MomentU[3] * MomentV[0]
                + a[2] * MomentU[2] * MomentV[1]
                + a[3] * ( MomentU[4] * MomentV[0] + MomentU[2] * MomentV[2] + MomentU[2] * MomentV[0] * MomentXi[2] )
                + b[0] * MomentU[1] * MomentV[1]
                + b[1] * MomentU[2] * MomentV[1]
                + b[2] * MomentU[1] * MomentV[2]
                + b[3] * ( MomentU[3] * MomentV[1] + MomentU[1] * MomentV[3] + MomentU[1] * MomentV[1] * MomentXi[2] );

    timeGrad[2] = a[0] * MomentU[1] * MomentV[1]
                + a[1] * MomentU[2] * MomentV[1]
                + a[2] * MomentU[1] * MomentV[2]
                + a[3] * ( MomentU[3] * MomentV[1] + MomentU[1] * MomentV[3] + MomentU[1] * MomentV[1] * MomentXi[2] )
                + b[0] * MomentU[0] * MomentV[2]
                + b[1] * MomentU[1] * MomentV[2]
                + b[2] * MomentU[0] * MomentV[3]
                + b[3] * ( MomentU[2] * MomentV[2] + MomentU[0] * MomentV[4] + MomentU[0] * MomentV[2] * MomentXi[2] );

    timeGrad[3] = a[0] * 0.50 * ( MomentU[3] * MomentV[0] + MomentU[1] * MomentV[2] + MomentU[1] * MomentV[0] * MomentXi[2] )
                + a[1] * 0.50 * ( MomentU[4] * MomentV[0] + MomentU[2] * MomentV[2] + MomentU[2] * MomentV[0] * MomentXi[2] )
                + a[2] * 0.50 * ( MomentU[3] * MomentV[1] + MomentU[1] * MomentV[3] + MomentU[1] * MomentV[1] * MomentXi[2] )
                + a[3] * 0.25 * ( MomentU[5] + MomentU[1]* ( MomentV[4] + MomentXi[4] )
                                + 2.0 * MomentU[3] * MomentV[2]
                                + 2.0 * MomentU[3] * MomentXi[2]
                                + 2.0 * MomentU[1] * MomentV[2] * MomentXi[2] )
                + b[0] * 0.50 * ( MomentU[2] * MomentV[1] + MomentU[0] * MomentV[3] + MomentU[0] * MomentV[1] * MomentXi[2] )
                + b[1] * 0.50 * ( MomentU[3] * MomentV[1] + MomentU[1] * MomentV[3] + MomentU[1] * MomentV[1] * MomentXi[2] )
                + b[2] * 0.50 * ( MomentU[2] * MomentV[2] + MomentU[0] * MomentV[4] + MomentU[0] * MomentV[2] * MomentXi[2] )
                + b[3] * 0.25 * ( MomentV[5] + MomentV[1] * ( MomentU[4] + MomentXi[4] )
                                + 2.0 * MomentU[2] * MomentV[3]
                                + 2.0 * MomentU[2] * MomentV[1] * MomentXi[2]
                                + 2.0 * MomentV[3] * MomentXi[2] );

    timeGrad[0] /= -prim[0];
    timeGrad[1] /= -prim[0];
    timeGrad[2] /= -prim[0];
    timeGrad[3] /= -prim[0];
}

void Interface::assembleFlux(double * MomentU, double * MomentV, double * MomentXi, double * a, double * b, double * A, double * timeCoefficients, double dy, double* prim)
{
    double Flux_1[4];
    double Flux_2[4];
    double Flux_3[4];

    Flux_1[0] = MomentU[1];
    Flux_1[1] = MomentU[2];
    Flux_1[2] = MomentU[1] * MomentV[1];
    Flux_1[3] = 0.5 * (MomentU[3] + MomentU[1] * MomentV[2] + MomentU[1] * MomentXi[2]);

    Flux_2[0] = ( a[0] * MomentU[2] 
                + a[1] * MomentU[3]
                + a[2] * MomentU[2] * MomentV[1]
                + a[3] * 0.5 * ( MomentU[4] + MomentU[2]*MomentV[2] + MomentU[2]*MomentXi[2] )
                + b[0] * MomentU[1] * MomentV[1]
                + b[1] * MomentU[2] * MomentV[1]
                + b[2] * MomentU[1] * MomentV[2]
                + b[3] * 0.5 * ( MomentU[3]*MomentV[1] + MomentU[1]*MomentV[3] + MomentU[1]*MomentV[1]*MomentXi[2] )
                );
    Flux_2[1] = ( a[0] * MomentU[3] 
                + a[1] * MomentU[4]
                + a[2] * MomentU[3] * MomentV[1]
                + a[3] * 0.5 * ( MomentU[5] + MomentU[3]*MomentV[2] + MomentU[3]*MomentXi[2] )
                + b[0] * MomentU[2] * MomentV[1]
                + b[1] * MomentU[3] * MomentV[1]
                + b[2] * MomentU[2] * MomentV[2]
                + b[3] * 0.5 * ( MomentU[4]*MomentV[1] + MomentU[2]*MomentV[3] + MomentU[2]*MomentV[1]*MomentXi[2] )
                );
    Flux_2[2] = ( a[0] * MomentU[2] * MomentV[1]
                + a[1] * MomentU[3] * MomentV[1]
                + a[2] * MomentU[2] * MomentV[2]
                + a[3] * 0.5 * ( MomentU[4]*MomentV[1] + MomentU[2]*MomentV[3] + MomentU[2]*MomentV[1]*MomentXi[2] )
                + b[0] * MomentU[1] * MomentV[2]
                + b[1] * MomentU[2] * MomentV[2]
                + b[2] * MomentU[1] * MomentV[3]
                + b[3] * 0.5 * ( MomentU[3]*MomentV[2] + MomentU[1]*MomentV[4] + MomentU[1]*MomentV[2]*MomentXi[2] )
                );
    Flux_2[3] = 0.5 * ( a[0] * ( MomentU[4] * MomentV[0] + MomentU[2] * MomentV[2] + MomentU[2] * MomentV[0] * MomentXi[2] )
                      + a[1] * ( MomentU[5] * MomentV[0] + MomentU[3] * MomentV[2] + MomentU[3] * MomentV[0] * MomentXi[2] )
                      + a[2] * ( MomentU[4] * MomentV[1] + MomentU[2] * MomentV[3] + MomentU[2] * MomentV[1] * MomentXi[2] )
                      + a[3] * ( 0.5 * ( MomentU[6] * MomentV[0] + MomentU[2] * MomentV[4] + MomentU[2] * MomentV[0] * MomentXi[4] )
                               +       ( MomentU[4] * MomentV[2] + MomentU[4] * MomentV[0] * MomentXi[2] + MomentU[2] * MomentV[2] * MomentXi[2] ) )
                      + b[0] * ( MomentU[3] * MomentV[1] + MomentU[1] * MomentV[3] + MomentU[1] * MomentV[1] * MomentXi[2] )
                      + b[1] * ( MomentU[4] * MomentV[1] + MomentU[2] * MomentV[3] + MomentU[2] * MomentV[1] * MomentXi[2] )
                      + b[2] * ( MomentU[3] * MomentV[2] + MomentU[1] * MomentV[4] + MomentU[1] * MomentV[2] * MomentXi[2] )
                      + b[3] * ( 0.5 * ( MomentU[5] * MomentV[1] + MomentU[1] * MomentV[5] + MomentU[1] * MomentV[1] * MomentXi[4] )
                               +       ( MomentU[3] * MomentV[3] + MomentU[3] * MomentV[1] * MomentXi[2] + MomentU[1] * MomentV[3] * MomentXi[2] ) )
                      );

    Flux_3[0] = ( A[0] * MomentU[1] * MomentV[0]
                + A[1] * MomentU[2] * MomentV[0]
                + A[2] * MomentU[1] * MomentV[1]
                + A[3] * 0.5 * ( MomentU[3]*MomentV[0] + MomentU[1]*MomentV[2] + MomentU[1]*MomentV[0]*MomentXi[2] )
                );
    Flux_3[1] = ( A[0] * MomentU[2] * MomentV[0]
                + A[1] * MomentU[3] * MomentV[0]
                + A[2] * MomentU[2] * MomentV[1]
                + A[3] * 0.5 * ( MomentU[4]*MomentV[0] + MomentU[2]*MomentV[2] + MomentU[2]*MomentV[0]*MomentXi[2] )
                );
    Flux_3[2] = ( A[0] * MomentU[1] * MomentV[1]
                + A[1] * MomentU[2] * MomentV[1]
                + A[2] * MomentU[1] * MomentV[2]
                + A[3] * 0.5 * ( MomentU[3]*MomentV[1] + MomentU[1]*MomentV[3] + MomentU[1]*MomentV[1]*MomentXi[2] )
                );
    Flux_3[3] = 0.5 * ( A[0] * ( MomentU[3] * MomentV[0] + MomentU[1] * MomentV[2] + MomentU[1] * MomentV[0] * MomentXi[2] )
                      + A[1] * ( MomentU[4] * MomentV[0] + MomentU[2] * MomentV[2] + MomentU[2] * MomentV[0] * MomentXi[2] )
                      + A[2] * ( MomentU[3] * MomentV[1] + MomentU[1] * MomentV[3] + MomentU[1] * MomentV[1] * MomentXi[2] )
                      + A[3] * ( 0.5 * ( MomentU[5] * MomentV[0] + MomentU[1] * MomentV[4] + MomentU[1] * MomentV[0] * MomentXi[4] )
                               +       ( MomentU[3] * MomentV[2] + MomentU[3] * MomentXi[2] + MomentU[1] * MomentV[2] * MomentXi[2] ) )
                      );

    for(int i = 0; i < 4; i++)
        this->Flux[i] = ( timeCoefficients[0]*Flux_1[i] + timeCoefficients[1]*Flux_2[i] + timeCoefficients[2]*Flux_3[i] ) * dy;
}

void Interface::rotate(double * vector)
{
    double tmp = vector[1];
    vector[1] = vector[2];
    vector[2] = tmp;
}

void Interface::computeMicroSlope(double * prim, double * macroSlope, double * microSlope)
{
    double A, B, C, U_2_V_2;

    U_2_V_2 = prim[1] * prim[1] + prim[2] * prim[2];

    A = 2.0*macroSlope[3] - ( U_2_V_2  + (this->fluidParam.K + 2.0) / (2.0*prim[3]) * macroSlope[0] );

    // the product rule of derivations is used here!
    B = macroSlope[1] - prim[1] * macroSlope[0];
    C = macroSlope[2] - prim[2] * macroSlope[0];

    // compute micro slopes of primitive variables from macro slopes of conservatice variables
    microSlope[3] = (4.0 * prim[3]*prim[3])/(this->fluidParam.K + 2.0)
                  * ( A - 2.0*prim[1]*B - 2.0*prim[2]*C );

    microSlope[2] = 2.0 * prim[3] * C - prim[2] * microSlope[3];

    microSlope[1] = 2.0 * prim[3] * B - prim[1] * microSlope[3];

    microSlope[0] = macroSlope[0] - prim[1]*microSlope[1] - prim[2]*microSlope[2] 
                                  - 0.5 * ( U_2_V_2 + (this->fluidParam.K + 2.0) / (2.0*prim[3]) )* microSlope[3];
}

void Interface::computeMoments(double * prim, double * MomentU, double* MomentV, double * MomentXi, int numberMoments)
{
    //==================== U Moments ==========================================
    MomentU[0] = 1.0;
    MomentU[1] = prim[1];
    for (int i = 2; i < numberMoments; i++)
        MomentU[i] = prim[1] * MomentU[i - 1] + (i - 1)/(2.0*prim[3])*MomentU[i - 2];

    //==================== V Moments ==========================================
    MomentV[0] = 1.0;
    MomentV[1] = prim[2];
    for (int i = 2; i < numberMoments; i++)
        MomentV[i] = prim[2] * MomentV[i - 1] + (i - 1)/(2.0*prim[3])*MomentV[i - 2];

    //==================== Xi Moments =========================================
    MomentXi[0] = 1.0;
    MomentXi[1] = 0.0;
    MomentXi[2] = this->fluidParam.K / (2.0 * prim[3]);
    MomentXi[3] = 0.0;
    MomentXi[4] = ( 2.0*this->fluidParam.K + 1.0*this->fluidParam.K*this->fluidParam.K ) / (4.0 * prim[3] * prim[3]);
    MomentXi[5] = 0.0;
    MomentXi[6] = 0.0;
}

