/*=========================================================================

  Program:   ParaView
  Module:    vtkPVContourEntry.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVContourEntry.h"

#include "vtkContourValues.h"
#include "vtkKWListBox.h"
#include "vtkObjectFactory.h"
#include "vtkPVAnimationInterfaceEntry.h"
#include "vtkPVApplication.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVArrayMenu.h"
#include "vtkPVContourWidgetProperty.h"
#include "vtkPVScalarRangeLabel.h"
#include "vtkPVSource.h"
#include "vtkPVXMLElement.h"

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVContourEntry);
vtkCxxRevisionMacro(vtkPVContourEntry, "1.47");

vtkCxxSetObjectMacro(vtkPVContourEntry, ArrayMenu, vtkPVArrayMenu);

//-----------------------------------------------------------------------------
int vtkPVContourEntryCommand(ClientData cd, Tcl_Interp *interp,
                        int argc, char *argv[]);


//-----------------------------------------------------------------------------
vtkPVContourEntry::vtkPVContourEntry()
{
  this->CommandFunction = vtkPVContourEntryCommand;

  this->SuppressReset = 1;
  
  this->ArrayMenu = NULL;
  
  this->SetNumberCommand = NULL;
  this->SetContourCommand = NULL;
}

//-----------------------------------------------------------------------------
vtkPVContourEntry::~vtkPVContourEntry()
{
  this->SetArrayMenu(NULL);
  this->SetSetNumberCommand(NULL);
  this->SetSetContourCommand(NULL);
}

//-----------------------------------------------------------------------------
int vtkPVContourEntry::ComputeWidgetRange()
{
  if (!this->ArrayMenu)
    {
    vtkErrorMacro("Array menu has not been set.");
    return 0;
    }

  vtkPVArrayInformation* ai = this->ArrayMenu->GetArrayInformation();
  if (ai == NULL || ai->GetName() == NULL)
    {
    return 0;
    }

  ai->GetComponentRange(0, this->WidgetRange);
  this->UseWidgetRange = 1;
  return 1;
}

//-----------------------------------------------------------------------------
void vtkPVContourEntry::AcceptInternal(vtkClientServerID sourceID)
{
  int i;
  int numContours;

  if (sourceID.ID == 0)
    {
    return;
    }

  this->Superclass::AcceptInternal(sourceID);

  numContours = this->ContourValues->GetNumberOfContours();

  char **cmds = new char*[numContours+1];
  int *numScalars = new int[numContours+1];

  this->UpdateProperty();
  
  cmds[0] = new char[20];
  sprintf(cmds[0], this->SetNumberCommand);
  numScalars[0] = 1;
  
  for (i = 0; i < numContours; i++)
    {
    cmds[i+1] = new char[9];
    sprintf(cmds[i+1], this->SetContourCommand);
    numScalars[i+1] = 2;
    }
  
  this->Property->SetVTKSourceID(sourceID);
  this->Property->SetVTKCommands(numContours+1, cmds, numScalars);
  this->Property->AcceptInternal();
  
  for (i = 0; i < numContours+1; i++)
    {
    delete [] cmds[i];
    }
  delete [] cmds;
  delete [] numScalars;
}

//-----------------------------------------------------------------------------
void vtkPVContourEntry::SaveInBatchScript(ofstream *file)
{
  vtkClientServerID sourceID = this->PVSource->GetVTKSourceID(0);

  int i;
  float value;
  int numContours;

  numContours = (this->Property->GetNumberOfScalars() - 1) / 2;

  *file << "  [$pvTemp" << sourceID.ID << " GetProperty ContourValues] "
        << "SetNumberOfElements " << numContours << endl;
  for (i = 0; i < numContours; i++)
    {
    value = this->Property->GetScalar(2*(i+1));
    *file << "  ";
    *file << "[$pvTemp" << sourceID.ID << " GetProperty ContourValues] "
          << "SetElement " << i << " " << value << endl;
    }
}

//-----------------------------------------------------------------------------
// If we had access to the ContourValues object of the filter,
// this would be much easier.  We would not have to rely on Tcl calls.
void vtkPVContourEntry::ResetInternal()
{
  int i;
  int numContours;
  
  if (this->PVSource == NULL)
    {
    vtkErrorMacro("PVSource not set.");
    return;
    }

  numContours = (this->Property->GetNumberOfScalars()-1)/2;
  float *scalars = this->Property->GetScalars();
  
  // The widget has been modified.  
  // Now set the widget back to reflect the contours in the filter.
  this->ContourValuesList->DeleteAll();
  this->ContourValues->SetNumberOfContours(0);
  for (i = 0; i < numContours; i++)
    {
    this->AddValue(scalars[2*(i+1)]);
    }

  // Since the widget now matches the fitler, it is no longer modified.
  if (this->AcceptCalled)
    {
    this->ModifiedFlag = 0;
    }
}

//-----------------------------------------------------------------------------
void vtkPVContourEntry::AnimationMenuCallback(vtkPVAnimationInterfaceEntry *ai)
{
  if (ai->InitializeTrace(NULL))
    {
    this->AddTraceEntry("$kw(%s) AnimationMenuCallback $kw(%s)", 
                        this->GetTclName(), ai->GetTclName());
    }
  
  ai->SetLabelAndScript(this->GetTraceName(), NULL, this->GetTraceName());
  ai->SetCurrentProperty(this->Property);
  if (this->UseWidgetRange)
    {
    ai->SetTimeStart(this->WidgetRange[0]);
    ai->SetTimeEnd(this->WidgetRange[1]);
    }
  ai->Update();
}

//----------------------------------------------------------------------------
void vtkPVContourEntry::CopyProperties(
  vtkPVWidget* clone, 
  vtkPVSource* pvSource,
  vtkArrayMap<vtkPVWidget*, vtkPVWidget*>* map)
{
  this->Superclass::CopyProperties(clone, pvSource, map);
  vtkPVContourEntry* pvce = vtkPVContourEntry::SafeDownCast(clone);
  if (pvce)
    {
    if (this->ArrayMenu)
      {
      // This will either clone or return a previously cloned
      // object.
      vtkPVArrayMenu* am = this->ArrayMenu->ClonePrototype(pvSource, map);
      pvce->SetArrayMenu(am);
      am->Delete();
      }
    pvce->SetSetNumberCommand(this->SetNumberCommand);
    pvce->SetSetContourCommand(this->SetContourCommand);
    }
  else 
    {
    vtkErrorMacro("Internal error. Could not downcast clone to PVContourEntry.");
    }
}

//-----------------------------------------------------------------------------
int vtkPVContourEntry::ReadXMLAttributes(vtkPVXMLElement* element,
                                         vtkPVXMLPackageParser* parser)
{
  if(!this->Superclass::ReadXMLAttributes(element, parser)) { return 0; }
  
  // Setup the ArrayMenu.
  const char* array_menu = element->GetAttribute("array_menu");
  if(array_menu)
    {
    vtkPVXMLElement* ame = element->LookupElement(array_menu);
    if (!ame)
      {
      vtkErrorMacro("Couldn't find ArrayMenu element " << array_menu);
      return 0;
      }
    vtkPVWidget* w = this->GetPVWidgetFromParser(ame, parser);
    vtkPVArrayMenu* amw = vtkPVArrayMenu::SafeDownCast(w);
    if(!amw)
      {
      if(w) { w->Delete(); }
      vtkErrorMacro("Couldn't get ArrayMenu widget " << array_menu);
      return 0;
      }
    amw->AddDependent(this);
    this->SetArrayMenu(amw);
    amw->Delete();  
    }

  this->SetSetNumberCommand(element->GetAttribute("set_number_command"));
  this->SetSetContourCommand(element->GetAttribute("set_contour_command"));
  
  return 1;
}

//-----------------------------------------------------------------------------
void vtkPVContourEntry::UpdateProperty()
{
  int numContours = this->ContourValues->GetNumberOfContours();
  float *scalars = new float[2*numContours+1];
  scalars[0] = numContours;
  int i;
  
  for (i = 0; i < numContours; i++)
    {
    scalars[2*i+1] = i;
    scalars[2*(i+1)] = this->ContourValues->GetValue(i);
    }
  this->Property->SetScalars(2*numContours+1, scalars);
  delete [] scalars;
}

//-----------------------------------------------------------------------------
void vtkPVContourEntry::SetProperty(vtkPVWidgetProperty *prop)
{
  this->Property = vtkPVContourWidgetProperty::SafeDownCast(prop);
}

//-----------------------------------------------------------------------------
vtkPVWidgetProperty* vtkPVContourEntry::GetProperty()
{
  return this->Property;
}

//-----------------------------------------------------------------------------
vtkPVWidgetProperty* vtkPVContourEntry::CreateAppropriateProperty()
{
  return vtkPVContourWidgetProperty::New();
}

//----------------------------------------------------------------------------
void vtkPVContourEntry::UpdateEnableState()
{
  this->Superclass::UpdateEnableState();

  this->PropagateEnableState(this->ArrayMenu);
}

//-----------------------------------------------------------------------------
void vtkPVContourEntry::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "ArrayMenu: " << this->GetArrayMenu() << endl;
  os << indent << "SetContourCommand: "
     << (this->SetContourCommand ? this->SetContourCommand : "(none)") << endl;
  os << indent << "SetNumberCommand: "
     << (this->SetNumberCommand ? this->SetNumberCommand : "(none)") << endl;
}
