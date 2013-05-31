/*
  GUIDO Library
  Copyright (C) 2002  Holger Hoos, Juergen Kilian, Kai Renz
  Copyright (C) 2004 Grame

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

  Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
  research@grame.fr

*/

#include <iostream>
#include <string.h>

#include "ARSymbol.h"

#include "GRDefine.h"
#include "FontManager.h"
#include "GRPage.h"
#include "GRRod.h"
#include "GRStaff.h"
#include "GRSymbol.h"
#include "GUIDOInternal.h"	// for gGlobalSettings.gDevice

#include "TagParameterFloat.h"
#include "VGDevice.h"
#include "VGFont.h"
#include "FontManager.h"

#include "Bitmap.h"

extern GRStaff * gCurStaff;

GRSymbol::GRSymbol(GRStaff * p_staff, ARSymbol * abstractRepresentationOfSymbol)
  : GRPTagARNotationElement(abstractRepresentationOfSymbol)
{
    assert(abstractRepresentationOfSymbol);

	GRSystemStartEndStruct * sse = new GRSystemStartEndStruct;
	GRSymbolSaveStruct * st = new GRSymbolSaveStruct;
	sse->p = (void *) st;
	sse->grsystem = p_staff->getGRSystem();
	sse->startflag = GRSystemStartEndStruct::LEFTMOST;
	mStartEndList.AddTail(sse);

	float curLSPACE = LSPACE;
	if (p_staff)
		curLSPACE = p_staff->getStaffLSPACE();

    st->baseFilePath = NVstring(); //REM: a corriger
    st->positionString = NVstring();
    st->bitmap = NULL;

    NVstring baseFilePath = abstractRepresentationOfSymbol->getPath();

    // - Set up file path
    if (abstractRepresentationOfSymbol->getSymbolPath())
    {
        st->filePath = abstractRepresentationOfSymbol->getSymbolPath();

        NVstring filePathString = st->filePath;

        // - Check in the current folder
        NVstring completePath(baseFilePath);
        completePath.append("/");
        completePath.append(filePathString);

        st->bitmap = new Bitmap(completePath.c_str());
        // -----------------------------

        if (!st->bitmap->getDevice())
        {
            // - Check in home directory
            NVstring completeHomePath("");

#ifdef WIN32
            // - Windows
            completeHomePath.append(getenv("HOMEDRIVE"));
            completeHomePath.append(getenv("HOMEPATH"));
            // ---------

#else
            // - Unix
            completeHomePath.append(getenv("HOME"));
            // ---------
#endif

            completeHomePath.append("\\");

            completeHomePath.append(filePathString);

            st->bitmap = new Bitmap(completeHomePath.c_str());
            // -------------------------

            if (!st->bitmap->getDevice())
            {
                // - Check if it's a hard path
                st->bitmap = new Bitmap(filePathString.c_str());
                // ---------------------------
            }
        }

        st->positionString = abstractRepresentationOfSymbol->getPositionString();

        float sizex = (float)st->bitmap->GetWidth();
        float sizey = (float)st->bitmap->GetHeight();

        float symbolSize = abstractRepresentationOfSymbol->getSize();

        st->boundingBox.right = sizex * symbolSize;
        st->boundingBox.top = sizey * symbolSize;
    }
    else
        st->filePath = "";

    st->boundingBox.left = 0;
    st->boundingBox.bottom = 0;
}


GRSymbol::~GRSymbol()
{
	assert(mStartEndList.empty());
	// this is important ...
	// All associaions that have been made
	// are dealt with in GRPositionTag ...

	// this makes sure, that we don't remove
	// associations, that are no longer there
	// after the Tag has been delete
	// (especially, if more than one system
	//  is handled.)
	delete mAssociated;
	mAssociated = 0;
}

void GRSymbol::OnDraw( VGDevice & hdc ) const
{
    GRSystemStartEndStruct * sse = getSystemStartEndStruct(gCurSystem);
    assert(sse);
    GRSymbolSaveStruct * st = (GRSymbolSaveStruct *) sse->p;

    if (st->bitmap)
    {
        const ARSymbol *arSymbol = getARSymbol();
        const float curLSPACE = gCurStaff ? gCurStaff->getStaffLSPACE(): LSPACE;

        NVPoint drawPos (st->position);

        float dx = 0;
        float dy = 0;

        if (arSymbol->getDY())
            dy = -arSymbol->getDY()->getValue( curLSPACE );
        if (arSymbol->getDX())
            dx = arSymbol->getDX()->getValue( curLSPACE );

        float currentSize = arSymbol->getSize();
        float positionStringDy;

        if (!st->positionString.compare("top"))
            positionStringDy = - st->bitmap->GetHeight() * currentSize - curLSPACE;
        else if (!st->positionString.compare("bot"))
            positionStringDy = 5 * curLSPACE;
        else //mid
            positionStringDy = 2 * curLSPACE - (st->bitmap->GetHeight() * currentSize / 2);

        float finaldx = drawPos.x + st->boundingBox.left + dx;
        float finaldy = dy + positionStringDy;

        // - Print image
        NVRect rectDraw = NVRect(finaldx, finaldy, finaldx + (float)st->bitmap->GetWidth() * currentSize, finaldy + (float)st->bitmap->GetHeight() * currentSize);
        st->bitmap->OnDraw(hdc, rectDraw);
    }
}

void GRSymbol::print() const
{
}

const ARSymbol * GRSymbol::getARSymbol() const
{
	return /*dynamic*/static_cast<const ARSymbol*>(getAbstractRepresentation());
}

void GRSymbol::addAssociation(GRNotationElement * el)
{
	GRNotationElement::addAssociation(el);
	GRPositionTag::addAssociation(el);
}

/** \brief Called directly by a spring. Then we know that we
	do not have a position tag.
*/
void GRSymbol::setPosition(const NVPoint & inPosition)
{
	GRPTagARNotationElement::setPosition(inPosition);

	// how do I get the current sse?

	// there can be only one sse! -> no overlap
	assert(mStartEndList.size() == 1);
	GRSystemStartEndStruct * sse = mStartEndList.GetHead();

	GRSymbolSaveStruct * st = (GRSymbolSaveStruct *) sse->p;
	assert(st);

	st->position = inPosition;
}

void GRSymbol::setHPosition( GCoord nx )
{
	GRPTagARNotationElement::setHPosition(nx);
	// there can be only one sse! -> no overlap
	assert(mStartEndList.size() == 1);
	GRSystemStartEndStruct * sse = mStartEndList.GetHead();

	GRSymbolSaveStruct * st = (GRSymbolSaveStruct *) sse->p;
	assert(st);

	st->position.x = nx;
}

void GRSymbol::tellPosition(GObject * caller, const NVPoint & inPosition)
{
	GRNotationElement * grel =  dynamic_cast<GRNotationElement *>(caller);
	if( grel == 0 ) return;

	GRStaff * staff = grel->getGRStaff();
	if( staff == 0 ) return;

	GRSystemStartEndStruct * sse = getSystemStartEndStruct(staff->getGRSystem());
	assert(sse);

	GRSymbolSaveStruct * st = (GRSymbolSaveStruct *)sse->p;
	GRNotationElement * startElement = sse->startElement;
	NVPoint newPos( inPosition );

	// - Check if we're left-opened
	if (sse->startflag == GRSystemStartEndStruct::OPENLEFT)
	{
		if (grel != startElement)
		{
			if (st->position.x == 0)
			{
				newPos.x -= LSPACE * 0.5f; // this is actually notebreite!
				st->position = newPos;
				st->filePath = "-";
			}
		}
	}

	// - Follows the y-position of the first element of the range (if any)
	else if (grel == startElement)
	{
		newPos.y = grel->getPosition().y;
		st->position = newPos;

		const ARSymbol * arSymbol = getARSymbol();
		//const char* text = arSymbol ? arSymbol->getText() : 0;
		//if (text) st->symbol = text;
	}
}

void GRSymbol::removeAssociation(GRNotationElement * el)
{
	GRPositionTag::removeAssociation(el);
	GRARNotationElement::removeAssociation(el);
}

GCoord GRSymbol::getLeftSpace() const
{
	GRSystemStartEndStruct * sse = mStartEndList.GetHead();
	if (!sse) return 0;

	GRSymbolSaveStruct * st = (GRSymbolSaveStruct *)sse->p;
	if (!st) return 0;

	return -st->boundingBox.left;
}

GCoord GRSymbol::getRightSpace() const
{
	GRSystemStartEndStruct * sse = mStartEndList.GetHead();
	if (!sse) return 0;

	GRSymbolSaveStruct * st = (GRSymbolSaveStruct *)sse->p;
	if (!st) return 0;

	return st->boundingBox.right;
}