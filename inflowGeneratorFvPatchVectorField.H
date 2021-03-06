/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright held by original author
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Class
    inflowGeneratorFvPatchVectorField

Description

SourceFiles
    inflowGeneratorFvPatchVectorField.C

\*---------------------------------------------------------------------------*/

#ifndef inflowGeneratorFvPatchVectorField_H
#define inflowGeneratorFvPatchVectorField_H

#include "Random.H"
#include "genericInflowGeneratorFvPatchVectorField.H"
#include "homogeneousTurbulence.H"
#include "hat.H"
#include "SLList.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                     Class inflowGeneratorFvPatchVectorField Declaration
\*---------------------------------------------------------------------------*/

template<class TurbulentSpot>
class inflowGeneratorFvPatchVectorField
:
    public genericInflowGeneratorFvPatchVectorField
{
    
    // Private data
    SLList<TurbulentSpot> vortons_;
    
    typename TurbulentSpot::Parameters param_;
    scalar Lspot_;
    scalar overlap_;
    vector n_;

    void initialize();
    

public:

    //- Runtime type information
    TypeName("inflowGenerator");


    // Constructors

        //- Construct from patch and internal field
        inflowGeneratorFvPatchVectorField
        (
            const fvPatch&,
            const DimensionedField<vector, volMesh>&
        );

        //- Construct from patch, internal field and dictionary
        inflowGeneratorFvPatchVectorField
        (
            const fvPatch&,
            const DimensionedField<vector, volMesh>&,
            const dictionary&
        );

        //- Construct by mapping given inflowGeneratorFvPatchVectorField
        //  onto a new patch
        inflowGeneratorFvPatchVectorField
        (
            const inflowGeneratorFvPatchVectorField&,
            const fvPatch&,
            const DimensionedField<vector, volMesh>&,
            const fvPatchFieldMapper&
        );

        //- Construct as copy
        inflowGeneratorFvPatchVectorField
        (
            const inflowGeneratorFvPatchVectorField&
        );

        //- Construct and return a clone
        virtual tmp<fvPatchField<vector> > clone() const
        {
            return tmp<fvPatchField<vector> >
            (
                new inflowGeneratorFvPatchVectorField(*this)
            );
        }

        //- Construct as copy setting internal field reference
        inflowGeneratorFvPatchVectorField
        (
            const inflowGeneratorFvPatchVectorField&,
            const DimensionedField<vector, volMesh>&
        );

        //- Construct and return a clone setting internal field reference
        virtual tmp<fvPatchField<vector> > clone
        (
            const DimensionedField<vector, volMesh>& iF
        ) const
        {
            return tmp<fvPatchField<vector> >
            (
                new inflowGeneratorFvPatchVectorField(*this, iF)
            );
        }


    // Member functions



        // Mapping functions

    /*
            //- Map (and resize as needed) from self given a mapping object
            virtual void autoMap
            (
                const fvPatchFieldMapper&
            );

            //- Reverse map the given fvPatchField onto this fvPatchField
            virtual void rmap
            (
                const fvPatchField<vector>&,
                const labelList&
            );
        */


        // Evaluation functions

        virtual void doUpdate();

        //- Write
        virtual void write(Ostream&) const;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //


#include "inflowGeneratorFvPatchVectorField.C"

#endif

// ************************************************************************* //
