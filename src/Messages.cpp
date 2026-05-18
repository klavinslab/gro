/////////////////////////////////////////////////////////////////////////////////////////
//
// gro is protected by the UW OPEN SOURCE LICENSE, which is summarized here.
// Please see the file LICENSE.txt for the complete license.
//
// THE SOFTWARE (AS DEFINED BELOW) AND HARDWARE DESIGNS (AS DEFINED BELOW) IS PROVIDED
// UNDER THE TERMS OF THIS OPEN SOURCE LICENSE (“LICENSE”).  THE SOFTWARE IS PROTECTED
// BY COPYRIGHT AND/OR OTHER APPLICABLE LAW.  ANY USE OF THIS SOFTWARE OTHER THAN AS
// AUTHORIZED UNDER THIS LICENSE OR COPYRIGHT LAW IS PROHIBITED.
//
// BY EXERCISING ANY RIGHTS TO THE SOFTWARE AND/OR HARDWARE PROVIDED HERE, YOU ACCEPT AND
// AGREE TO BE BOUND BY THE TERMS OF THIS LICENSE.  TO THE EXTENT THIS LICENSE MAY BE
// CONSIDERED A CONTRACT, THE UNIVERSITY OF WASHINGTON (“UW”) GRANTS YOU THE RIGHTS
// CONTAINED HERE IN CONSIDERATION OF YOUR ACCEPTANCE OF SUCH TERMS AND CONDITIONS.
//
// TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION
//
//

#include "Micro.h"

MessageHandler::MessageHandler ( void ) {

}

MessageHandler::~MessageHandler ( void ) {

}

void MessageHandler::add_message ( int i, const char * str ) {

  ASSERT ( i >=0 && i < 4 );

  message_buffer[i].push_front ( str );

  if ( message_buffer[i].size() > 10 )
    message_buffer[i].pop_back();

}
 
void MessageHandler::clear_messages ( int i ) { // clears a quadrant

  while ( message_buffer[i].size() > 0 ) 
    message_buffer[i].pop_back();

}

void MessageHandler::clear_messages_all ( void ) { // clears all quadrants

  for ( int i=0; i<4; i++ )
    clear_messages ( i );

}

#ifndef NOGUI
void MessageHandler::render ( GroPainter * painter ) {

  std::list<std::string>::iterator k;
  int offset;
  int w = painter->get_size().width()/2,
      h = painter->get_size().height()/2;

  // lower left /////////////////////////////////////////////////////////////////////////////////////

  offset = 16*message_buffer[0].size()+10;

  for ( k=message_buffer[0].begin(); k != message_buffer[0].end(); k++ ) {

      painter->drawText ( QRect ( QPoint ( -w+10, h-offset ), QSize ( w-10, 20 ) ),
                          QString ( (*k).c_str() ),
                          QTextOption ( Qt::AlignLeft ) );
      offset -= 16;

  }

  // lower right /////////////////////////////////////////////////////////////////////////////////////

  offset = 16*message_buffer[1].size()+10;

  for ( k=message_buffer[1].begin(); k != message_buffer[1].end(); k++ ) {

      painter->drawText ( QRect ( QPoint ( 0, h-offset ), QSize ( w-10, 20 ) ),
                          QString ( (*k).c_str() ),
                          QTextOption ( Qt::AlignRight ) );
      offset -= 16;

  }

  // upper right /////////////////////////////////////////////////////////////////////////////////////

  offset = 10;

  for ( k=message_buffer[2].begin(); k != message_buffer[2].end(); k++ ) {

      painter->drawText ( QRect ( QPoint ( 0, -h+offset ), QSize ( w-10, 20 ) ),
                          QString ( (*k).c_str() ),
                          QTextOption ( Qt::AlignRight ) );
      offset += 16;

  }

}
#endif
