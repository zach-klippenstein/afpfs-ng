/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

#include <kdebug.h>



void afploginWidgetBase::authentication_specific_clicked()
{
    printf("specific authentication selected\n");
    uam->setEnabled(true);
}


void afploginWidgetBase::authentication_guest_clicked()
{
    uam->setEnabled(false);
}


void afploginWidgetBase::authentication_mostsecure_clicked()
{
    uam->setEnabled(false);
}


void afploginWidgetBase::volume_list_selectionChanged()
{
    attach->setEnabled(true);
}
