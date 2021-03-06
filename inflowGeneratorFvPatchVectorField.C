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
    Generates turbulent fluctuations using Nikolai Kornev's method of turbulent 
    spots.

    Parallelisation is archieved by holding the vorton list on the master
    processor and distribute the modified list on each timestep to all
    slave processors.

SourceFiles


\*---------------------------------------------------------------------------*/

#include "inflowGeneratorFvPatchVectorField.H"
#include "transform.H"
#include "transformField.H"
#include "volFields.H"
#include "ListListOps.H"
#include "PstreamReduceOps.H"
#include "addToRunTimeSelectionTable.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

template<class TurbulentSpot>
void inflowGeneratorFvPatchVectorField<TurbulentSpot>::initialize()
{
    if (size()>0)
    {
        n_=average(patch().nf());
        
        if (mag(n_)<SMALL)
        {
            FatalErrorIn("inflowGeneratorFvPatchVectorField::updateCoeffs()")
                << "Could not determine patch normal direction"
                    << abort(FatalError);
        }
        
        n_/=mag(n_);
        
    }

    Lspot_=TurbulentSpot::calcInfluenceLength(param_);
    //Info<<"Influence length Lspot="<<Lspot_<<endl;
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class TurbulentSpot>
inflowGeneratorFvPatchVectorField<TurbulentSpot>::inflowGeneratorFvPatchVectorField
(
    const fvPatch& p,
    const DimensionedField<vector, volMesh>& iF
)
:
    genericInflowGeneratorFvPatchVectorField(p, iF),
    overlap_(0.5)
{
    initialize();
}

template<class TurbulentSpot>
inflowGeneratorFvPatchVectorField<TurbulentSpot>::inflowGeneratorFvPatchVectorField
(
    const inflowGeneratorFvPatchVectorField& ptf,
    const fvPatch& p,
    const DimensionedField<vector, volMesh>& iF,
    const fvPatchFieldMapper& mapper
)
:
    genericInflowGeneratorFvPatchVectorField(ptf, p, iF, mapper),
    vortons_(ptf.vortons_),
    param_(ptf.param_),
    overlap_(0.5)
{
    initialize();
}

template<class TurbulentSpot>
inflowGeneratorFvPatchVectorField<TurbulentSpot>::inflowGeneratorFvPatchVectorField
(
    const fvPatch& p,
    const DimensionedField<vector, volMesh>& iF,
    const dictionary& dict
)
:
    genericInflowGeneratorFvPatchVectorField(p, iF, dict),
    param_(dict),
    overlap_(readScalar(dict.lookup("overlap")))
{
    initialize();
    if (dict.found("vortons"))
    {
        vortons_=SLList<TurbulentSpot>(dict.lookup("vortons"));
    }
}

template<class TurbulentSpot>
inflowGeneratorFvPatchVectorField<TurbulentSpot>::inflowGeneratorFvPatchVectorField
(
    const inflowGeneratorFvPatchVectorField& ptf
)
:
    genericInflowGeneratorFvPatchVectorField(ptf),
    vortons_(ptf.vortons_),
    param_(ptf.param_),
    Lspot_(ptf.Lspot_),
    overlap_(ptf.overlap_),
    n_(ptf.n_)
{}

template<class TurbulentSpot>
inflowGeneratorFvPatchVectorField<TurbulentSpot>::inflowGeneratorFvPatchVectorField
(
    const inflowGeneratorFvPatchVectorField& ptf,
    const DimensionedField<vector, volMesh>& iF
)
:
    genericInflowGeneratorFvPatchVectorField(ptf, iF),
    vortons_(ptf.vortons_),
    param_(ptf.param_),
    Lspot_(ptf.Lspot_),
    overlap_(ptf.overlap_),
    n_(ptf.n_)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

    /*
template<class TurbulentSpot>
void inflowGeneratorFvPatchVectorField<TurbulentSpot>::autoMap
(
    const fvPatchFieldMapper& m
)
{
    Field<vector>::autoMap(m);
    referenceField_.autoMap(m);
}


template<class TurbulentSpot>
void inflowGeneratorFvPatchVectorField<TurbulentSpot>::rmap
(
    const fvPatchField<vector>& ptf,
    const labelList& addr
)
{
    fixedValueFvPatchField<vector>::rmap(ptf, addr);

    const inflowGeneratorFvPatchVectorField& tiptf =
        refCast<const inflowGeneratorFvPatchVectorField >(ptf);

    referenceField_.rmap(tiptf.referenceField_, addr);
}
        */

template<class TurbulentSpot>
void inflowGeneratorFvPatchVectorField<TurbulentSpot>::doUpdate()
{
    Field<vector> tloc;
    vector rotbase(pTraits<vector>::zero);
    tensor rot(pTraits<tensor>::zero), rotback(pTraits<tensor>::zero);

    Field<vector>& patchField = *this;

    // transform face centres to local frame
    rotbase=gAverage(patch().Cf());
    if (size()>0)
    {
    
        rot=rotationTensor(n_, -vector(0,0,1));
        rotback=inv(rot);
        tloc=transform(rot, patch().Cf()-rotbase);

    }
    
    // ==================== Generation of new spots ========================
    boundBox bb(tloc);
    
    List<List<TurbulentSpot> > scatterList(Pstream::nProcs());

    if ( Pstream::master() )
    {
        label n_x=label(ceil((bb.max().x()-bb.min().x())/(overlap_*Lspot_)));
        label n_y=label(ceil((bb.max().y()-bb.min().y())/(overlap_*Lspot_)));

        scalar dx=ranGen_.scalar01() * overlap_ * Lspot_;
        scalar dy=ranGen_.scalar01() * overlap_ * Lspot_;
        
        for (label i=0;i<n_x;i++)
        {
            for (label j=0;j<n_y;j++)
            {
                vector x
                    (
                        bb.min().x() + scalar(i)*Lspot_ + dx,
                        bb.min().y() + scalar(j)*Lspot_ + dy,
                        -Lspot_
                    );

                bool keep=true;
                for (typename SLList<TurbulentSpot>::iterator vorton
                         = vortons_.begin(); vorton!=vortons_.end(); ++vorton)
                {
                    if (mag(vorton().location() - x) < overlap_*Lspot_)
                    {
                        keep=false;
                        break;
                    }
                }

                if (keep)
                {
                    TurbulentSpot newvorton(x);
                    newvorton.randomize(ranGen_);
                    vortons_.insert(newvorton);
                }
            }
        }

        scatterList[Pstream::myProcNo()].setSize(vortons_.size());
        // transfer vortons to scatterList
        label i=0;
        for (typename SLList<TurbulentSpot>::iterator vorton
                 = vortons_.begin(); vorton!=vortons_.end(); ++vorton)
            scatterList[Pstream::myProcNo()][i++]=vorton();
    }

    // Distribute vortons to all processors
    Pstream::scatterList(scatterList);
    
    List<TurbulentSpot> allvortons= 
        ListListOps::combine<List<TurbulentSpot> >
        (
            scatterList,
            accessOp<List<TurbulentSpot> >()
        );

    // =========== Calculation of induced turbulent fluctuations ==========
    if (size()>0)
    {
        Field<vector> turbulent(referenceField_.size(), pTraits<vector>::zero);

        forAll(patchField, I)
        {
            vector pt=tloc[I];
            
            forAll(allvortons, vI)
            {
                const TurbulentSpot& vorton=allvortons[vI];

                if (mag(vorton.location() - pt) < Lspot_)
                {
                    turbulent[I]+=
                        cmptMultiply
                        (
                            fluctuationScale_,
                            transform(rotback, vorton.fluctuation(param_, pt))
                        );
                }
            }
        }

        tensorField a(turbulent.size(), pTraits<tensor>::zero);
	a.replace(tensor::XX, sqrt(RField_.component(symmTensor::XX)));
	a.replace(tensor::YX, RField_.component(symmTensor::XY)/a.component(tensor::XX));
	a.replace(tensor::ZX, RField_.component(symmTensor::XZ)/a.component(tensor::XX));
	a.replace(tensor::YY, sqrt(RField_.component(symmTensor::YY)-sqr(a.component(tensor::YX))));
	a.replace(tensor::ZY, ( RField_.component(symmTensor::YZ)
             -a.component(tensor::YX)*a.component(tensor::ZX) )/a.component(tensor::YY));
        a.replace(tensor::ZZ, sqrt(RField_.component(symmTensor::ZZ)
             -sqr(a.component(tensor::ZX))-sqr(a.component(tensor::ZY))));

        fixedValueFvPatchField<vector>::operator==(referenceField_+ (a&turbulent) );
    }

    scalar um=gAverage(referenceField_ & (-patch().nf()) );

    if (Pstream::master())
    {
        // =================== Vorton motion =======================
        
        if (um*this->db().time().deltaT().value() > 0.5*Lspot_)
        {
            WarningIn("inflowGeneratorFvPatchVectorField::updateCoeffs()")
                << "timestep to large: spots pass inlet within one timestep"<<endl;
        }
        
        for (typename SLList<TurbulentSpot>::iterator vorton
                 = vortons_.begin(); vorton!=vortons_.end(); ++vorton)
        {
            vorton().moveForward(um*this->db().time().deltaT().value()*vector(0,0,1));
        }        
        
        // ================ remove Vortons that left inlet plane =====================
        
        bool modified;
        do
        {
            modified=false;
            for (typename SLList<TurbulentSpot>::iterator vorton
                     = vortons_.begin(); vorton!=vortons_.end(); ++vorton)
            {
                if (vorton().location().z() > Lspot_) 
                {
                    vortons_.remove(vorton);
                    modified=true;
                    break;
                }
            } 
        } while (modified);
        
        Pout<<"Number of turbulent spots: "<<vortons_.size()<<endl;
    }
}


template<class TurbulentSpot>
void inflowGeneratorFvPatchVectorField<TurbulentSpot>::write(Ostream& os) const
{
    os.writeKeyword("overlap")
        << overlap_ << token::END_STATEMENT <<nl;
    param_.write(os);

    genericInflowGeneratorFvPatchVectorField::write(os);

    if (Pstream::master())
    {
        os.writeKeyword("vortons")
            << vortons_ << token::END_STATEMENT <<nl;
    }
}

/*
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

    #define makeInflowGeneratorFvPatchField(spotType)                   \
                                                                        \
typedef inflowGeneratorFvPatchVectorField<spotType>                     \
inflowGeneratorFvPatchVectorField##spotType;                            \
                                                                        \
defineTemplateTypeNameAndDebugWithName(                                 \
    inflowGeneratorFvPatchVectorField##spotType,                        \
    "inflowGenerator<"#spotType">", 0)                                  \
                                                                        \
    addToRunTimeSelectionTable                                          \
(                                                                       \
    fvPatchVectorField,                                                 \
    inflowGeneratorFvPatchVectorField##spotType,                        \
    patch                                                               \
);                                                                      \
                                                                        \
addToRunTimeSelectionTable                                              \
(                                                                       \
    fvPatchVectorField,                                                 \
    inflowGeneratorFvPatchVectorField##spotType,                        \
    dictionary                                                          \
);                                                                      \
                                                                        \
addToRunTimeSelectionTable                                              \
(                                                                       \
    fvPatchVectorField,                                                 \
    inflowGeneratorFvPatchVectorField##spotType,                        \
    patchMapper                                                         \
)                                                                       \
    

makeInflowGeneratorFvPatchField(hat);
makeInflowGeneratorFvPatchField(homogeneousTurbulence);
*/

} // End namespace Foam

// ************************************************************************* //
