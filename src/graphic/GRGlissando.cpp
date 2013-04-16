#include <cstdlib>
#include "VGDevice.h"

#include "ARGlissando.h"
#include "TagParameterString.h"
#include "TagParameterFloat.h"
#include "GRStaff.h"
#include "GRGlue.h"
#include "GRSingleNote.h"
#include "GRGlobalStem.h"
#include "GRRest.h"
#include "GREmpty.h"
#include "GRChord.h"
#include "GRStdNoteHead.h"
#include "GraphTools.h"

#include "GRGlissando.h"

extern GRSystem * gCurSystem;

using namespace std;

// -----------------------------------------------------------------------------
GRGlissando::GRGlissando(GRStaff * grstaff, GRNotationElement * startEl,
					GRNotationElement * endEl)
			: GRPTagARNotationElement(new ARGlissando, 1)
{
	GRSystemStartEndStruct * sse = initGRGlissando( grstaff );

	if (startEl)
	{
		setStartElement(grstaff, startEl);
	}
	else // no start element: we're left-opened
	{
		setStartElement(grstaff, (grstaff->getSecondGlue()));
		sse->startflag = GRSystemStartEndStruct::OPENLEFT;
	}

	if (endEl)
	{
		setEndElement(grstaff,endEl);
		sse->endflag = GRSystemStartEndStruct::RIGHTMOST;
	}
	else  // no end element: we're righ-opened
	{
		setEndElement(grstaff, /*dynamic cast<GRNotationElement *>*/(grstaff->getEndGlue()));
		sse->endflag = GRSystemStartEndStruct::OPENRIGHT;
	}

	// - Get the time position.
	GRNotationElement * endElement;
	GRNotationElement * startElement;
	if(( endElement = getEndElement(grstaff)) != 0 )
	{
		mRelativeTimePositionOfGR = endElement->getRelativeTimePosition();
	}
	else if((startElement = getStartElement(grstaff)) != 0 )
	{
		mRelativeTimePositionOfGR = startElement->getRelativeTimePosition();
	}


	mBoundingBox.Set( 0, 0, 0, 0 );

}

// -----------------------------------------------------------------------------
GRGlissando::GRGlissando(GRStaff * grstaff)
	: GRPTagARNotationElement( new ARGlissando, true ) // ownsAR
{
	initGRGlissando( grstaff );
}

// -----------------------------------------------------------------------------
GRGlissando::GRGlissando(GRStaff * grstaff,
				   ARGlissando * abstractRepresentationOfGlissando)
				   : GRPTagARNotationElement(abstractRepresentationOfGlissando)
{
	assert(abstractRepresentationOfGlissando);
	initGRGlissando( grstaff );
}

GRGlissando::~GRGlissando()
{
	assert(mStartEndList.GetCount() == 0);
	FreeAssociatedList();
}

void GRGlissando::print() const
{
}

//� faire
void GRGlissando::OnDraw( VGDevice & hdc ) const
{
	
	if (error) return;

	assert( gCurSystem );

	GRSystemStartEndStruct * sse = getSystemStartEndStruct( gCurSystem );
	if( sse == 0)
		return; // don't draw

	GRGlissandoSaveStruct * glissInfos = (GRGlissandoSaveStruct *)sse->p;
	assert(glissInfos);

	if (mColRef) 
		hdc.PushFillColor( VGColor( mColRef ) );
	
	if(!wavy)
	{
		float coorX[4] = {glissInfos->points[0].x, glissInfos->points[1].x, glissInfos->points[2].x, glissInfos->points[3].x};
		float coorY[4] = {glissInfos->points[0].y, glissInfos->points[1].y, glissInfos->points[2].y, glissInfos->points[3].y};
		hdc.Polygon(coorX, coorY, 4);
	}
	else
	{
		float width = glissInfos->points[3].x - glissInfos->points[0].x;
		float height = glissInfos->points[3].y - glissInfos->points[0].y;
		float pasX = width/10;
		float pasY = height/10;

		float X = glissInfos->points[0].x;
		float Y = glissInfos->points[0].y;
		const int NSEGS = 25;

		for(int i=0; i<10; i++)
		{
			NVPoint thePoints[ 2*(NSEGS+3) ];
			int index = 0;
			float x1 = X+i*pasX;
			float y1 = Y+i*pasY;
			float y2a;
			float y2b;
			float x3 = X+pasX*(i+1);
			float y3 = Y+pasY*(i+1);
			if(i%2==0)
			{
				y2a = y1 - (pasY + pasX*pasX/pasY)/2;
				y2b = y3 - (pasY + pasX*pasX/pasY)/2;
			}
			else
			{
				y2a = y1 + (pasY + pasX*pasX/pasY)/2;
				y2b = y3 + (pasY + pasX*pasX/pasY)/2;
				
				x1-=1;
				x3+=1;
			}
			makeCurve(x1, y1, x1, y2a, x3, y2b, x3, y3, NSEGS, thePoints, &index);
			
			if(i%2==0)
			{
				y2a-=5;
				y2b-=5;
				x3-=1;
				x1+=1;
			}
			else
			{
				y2a+=5;
				y2b+=5;
				x1+=1;
				x3-=1;
			}
			
			makeCurve(x3, y3, x3, y2b, x1, y2a, x1, y1, NSEGS, thePoints, &index);
			float xPoints [ 2*(NSEGS+3) ];
			float yPoints [ 2*(NSEGS+3) ];
			for( int currPt = 0; currPt < index; ++ currPt )
			{
				xPoints [ currPt ] = thePoints[ currPt ].x;
				yPoints [ currPt ] = thePoints[ currPt ].y;
			}

			hdc.Polygon( xPoints, yPoints, index );
			
		}
	}
	if (mColRef) hdc.PopFillColor();

}

GRSystemStartEndStruct * GRGlissando::prepareSSEStructForGlissando( const GRStaff * inStaff )
{
	GRSystemStartEndStruct * sse = getSystemStartEndStruct( inStaff->getGRSystem());
	if( sse == 0 ) return 0;
	if (sse->endflag == GRSystemStartEndStruct::NOTKNOWN)
	{
		// this is not good ...
		setError(1);
		setStartElement(inStaff,NULL);
		setEndElement(inStaff,NULL);
		return 0 ;
	}
	return sse;
}

void GRGlissando::tellPosition(GObject * caller, const NVPoint & newPosition)
{
	GRNotationElement * grel = dynamic_cast<GRNotationElement *>(caller);
	if (grel == 0 ) return;

	GRStaff * staff = grel->getGRStaff();
	if (staff == 0 ) return;

	GRSystemStartEndStruct * sse = getSystemStartEndStruct( staff->getGRSystem());
	if (sse == 0)	return;

	const GRNotationElement * const startElement = sse->startElement;
	const GRNotationElement * const endElement = sse->endElement;

	// if ( openLeftRange && openRightRange ) return;
		// updateBow();

	if( grel == endElement || ( endElement == 0 && grel == startElement))
	{
		updateGlissando( staff );
	}
}

void GRGlissando::updateGlissando( GRStaff * inStaff )
{
	
	GRSystemStartEndStruct * sse = prepareSSEStructForGlissando( inStaff );
	if ( sse == 0 ) return;

	// --- Collects informations about the context ---

	GRGlissandoContext glissContext;
	glissContext.staff = inStaff;
	getGlissandoBeginingContext( &glissContext, sse );
	getGlissandoEndingContext( &glissContext, sse );

	GRNotationElement * startElement = sse->startElement;
	GRNotationElement * endElement = sse->endElement;
	GRGlissandoSaveStruct * glissInfos = (GRGlissandoSaveStruct *)sse->p;

	ARGlissando * arGliss = static_cast<ARGlissando *>(getAbstractRepresentation());
	const float staffLSpace = inStaff->getStaffLSPACE();
	assert(arGliss);

	float dx1 = arGliss->getDx1()->getValue( staffLSpace );
	float dy1 = arGliss->getDy1()->getValue( staffLSpace );
	float dx2 = arGliss->getDx2()->getValue( staffLSpace );
	float dy2 = arGliss->getDy2()->getValue( staffLSpace );

	float XLeft = 0;
	float YLeft = 0;
	float XRight = 0;
	float YRight = 0;

	if(glissContext.bottomLeftHead)
		{
			XLeft = glissContext.bottomLeftHead->getPosition().x + glissContext.bottomLeftHead->getBoundingBox().Width() + dx1;
			YLeft = glissContext.bottomLeftHead->getPosition().y - dy1;
		}
	if(glissContext.bottomRightHead)
		{
			XRight = glissContext.bottomRightHead->getPosition().x - glissContext.bottomRightHead->getBoundingBox().Width() + dx2;
			YRight = glissContext.bottomRightHead->getPosition().y - dy2;
		}
	float deltaX = XRight - XLeft;
	float  deltaY;
	if(YRight>=YLeft)
		deltaY = YRight - YLeft;
	else
		deltaY = YLeft - YRight;

	float thickness = arGliss->getThickness()->getValue( staffLSpace )*sqrt(deltaX*deltaX + deltaY*deltaY)/deltaX;

	glissInfos->points[0].x = glissInfos->points[1].x = XLeft;
	glissInfos->points[0].y = YLeft + thickness/2;
	glissInfos->points[1].y = YLeft - thickness/2;
	glissInfos->points[3].x = glissInfos->points[2].x = XRight;
	glissInfos->points[3].y = YRight + thickness/2;
	glissInfos->points[2].y = YRight - thickness/2;

	wavy = arGliss->isWavy();

	if (sse->startflag == GRSystemStartEndStruct::OPENLEFT || !startElement)
	{
		glissInfos->position.x = staffLSpace;
		glissInfos->position.y = inStaff->getBoundingBox().bottom - inStaff->getBoundingBox().Height()/2;
		glissInfos->points[0] = glissInfos->position;
		glissInfos->points[1].x = glissInfos->points[0].x;
		glissInfos->points[1].y = glissInfos->points[0].y;
	}
	
	if (sse->endflag == GRSystemStartEndStruct::OPENRIGHT || !endElement)
	{
		glissInfos->points[3].x = inStaff->getEndGlue()->getPosition().x;
		glissInfos->points[3].y = 20;
		glissInfos->points[2].x = glissInfos->points[3].x;
		glissInfos->points[2].y = glissInfos->points[3].y;
		
	}

	//updateBoundingBox();
}

//� faire
void GRGlissando::updateBoundingBox()
{
}	

void GRGlissando::getGlissandoBeginingContext( GRGlissandoContext * ioContext, GRSystemStartEndStruct * sse )
{
	GRNotationElement * startElement = sse->startElement;

	GRSingleNote * note = dynamic_cast<GRSingleNote *>(startElement);
	if( note )
	{
		ioContext->bottomLeftHead = note->getNoteHead();
		ioContext->topLeftHead = NULL;

	}
	else
	{
		GRGlobalStem * stem = findGlobalStem( sse, startElement );
		if( stem )
		{
			stem->getHighestAndLowestNoteHead( &ioContext->topLeftHead, &ioContext->bottomLeftHead );
		}
	}
}

void GRGlissando::getGlissandoEndingContext( GRGlissandoContext * ioContext, GRSystemStartEndStruct * sse )
{
	GRNotationElement * endElement = sse->endElement;

	GRSingleNote * note = dynamic_cast<GRSingleNote *>(endElement);
	if( note )
	{
		ioContext->bottomRightHead = note->getNoteHead();
		ioContext->topRightHead = NULL;
	}
	else
	{
		GRGlobalStem * stem = findGlobalStem( sse, endElement );
		if( stem )
		{
			stem->getHighestAndLowestNoteHead( &ioContext->topRightHead, &ioContext->bottomRightHead );
		}
	}
}

void GRGlissando::removeAssociation(GRNotationElement * el )
{
	GRPositionTag::removeAssociation(el);
	GRARNotationElement::removeAssociation(el);
}
void GRGlissando::addAssociation(GRNotationElement * grnot)
{

	if (error) return;

		if ( GREvent::cast( grnot )  && 	// stop immediately if it's not an event.
		(dynamic_cast<GRNote *>(grnot) ||
		 dynamic_cast<GRRest *>(grnot) ||
		 dynamic_cast<GRChord *>(grnot) ||
		 dynamic_cast<GREmpty *>(grnot)))
	{
	  	GRARNotationElement::addAssociation(grnot);
	}
	else
	{
		setError(1);
	}

	if (!error)
	{
		GRPositionTag::addAssociation(grnot);
	}
}

GRSystemStartEndStruct * GRGlissando::initGRGlissando( GRStaff * grstaff )
{
	assert(grstaff);

	setGRStaff( grstaff );// TEST

	GRSystemStartEndStruct * sse = new GRSystemStartEndStruct;
	sse->grsystem = grstaff->getGRSystem();
	sse->startflag = GRSystemStartEndStruct::LEFTMOST;
	mStartEndList.AddTail(sse);

	
	GRGlissandoSaveStruct * st = new GRGlissandoSaveStruct;
	st->numPoints = 4;

	// set Standard-Offsets
	//st->points[0].x = 0;
	//st->points[0].y = 0;

	sse->p = (void *)st;
	
	// mBoundingBox.Set( 0, 0, 0, 0 );

	return sse;
}

// -----------------------------------------------------------------------------
GRNotationElement * GRGlissando::getStartElement(GRStaff * grstaff) const
{
	GuidoPos pos = mStartEndList.GetHeadPosition();
	while (pos)
	{
		GRSystemStartEndStruct * sse = mStartEndList.GetNext(pos);
		if (sse->grsystem == grstaff->getGRSystem())
			return sse->startElement;
	}
	return 0;
}

// -----------------------------------------------------------------------------
GRNotationElement * GRGlissando::getEndElement(GRStaff * grstaff) const
{
	GuidoPos pos = mStartEndList.GetHeadPosition();
	while (pos)
	{
		GRSystemStartEndStruct * sse = mStartEndList.GetNext(pos);
		if (sse->grsystem == grstaff->getGRSystem())
			return sse->endElement;
	}
	return 0;
}

GRGlobalStem * GRGlissando::findGlobalStem( GRSystemStartEndStruct * sse, GRNotationElement * stemOwner )
{
	const NEPointerList * ptlist1 = stemOwner->getAssociations();
	if (ptlist1)
	{
		GuidoPos nepos = ptlist1->GetHeadPosition();
		while (nepos)
		{
			GRGlobalStem * stem = dynamic_cast<GRGlobalStem *>(ptlist1->GetNext(nepos));
			if (stem)
				return stem;
		}
	}
	return 0;
}

