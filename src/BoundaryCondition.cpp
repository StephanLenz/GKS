#include "BoundaryCondition.h"



BoundaryCondition::BoundaryCondition()
{
}

BoundaryCondition::BoundaryCondition( int rhoType, int UType, int VType, int TType,
                                      double rho, double U, double V, double T)
{
    this->type[0] = rhoType;
    this->type[1] = UType;
    this->type[2] = VType;
    this->type[3] = TType;

    this->value[0] = rho;
    this->value[1] = U;
    this->value[2] = V;
    this->value[3] = T;
}


BoundaryCondition::BoundaryCondition(double U, double V)
{
    this->type[0] = 0;
    this->type[1] = 0;
    this->type[2] = 0;
    this->type[3] = 0;

    this->value[0] = 0.0;
    this->value[1] = U;
    this->value[2] = V;
    this->value[3] = 0.0;
}

BoundaryCondition::~BoundaryCondition()
{
}

short int BoundaryCondition::getType(short int i)
{
    return this->type[i];
}

double BoundaryCondition::getValue(short int i)
{
    return this->value[i];
}
