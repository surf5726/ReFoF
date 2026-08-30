#ifndef FOF_USERMESSAGES_H
#define FOF_USERMESSAGES_H
#ifdef _WIN32
#pragma once
#endif // FOF_USERMESSAGES_H

class bf_read;

void FoFUserMessage_ShowMenuFoF( bf_read &msg );
void FoFUserMessage_FoFHint( bf_read &msg );
void FoFUserMessage_Cash( bf_read &msg );
void FoFUserMessage_Notoriety( bf_read &msg );
void FoFUserMessage_FoFSlide( bf_read &msg );
void FoFUserMessage_HudEquipItems( bf_read &msg );
void FoFUserMessage_CourseHint( bf_read &msg );
void FoFUserMessage_MaxHP( bf_read &msg );
void FoFUserMessage_IconComm( bf_read &msg );
void FoFUserMessage_HudCircleProgressBar( bf_read &msg );
void FoFUserMessage_CapMessage( bf_read &msg );
void FoFUserMessage_HitRecon( bf_read &msg );
void FoFUserMessage_HitReconRank( bf_read &msg );
void FoFUserMessage_HUDBBMulti( bf_read &msg );
void FoFUserMessage_BBNotices( bf_read &msg );
void FoFUserMessage_HitBow( bf_read &msg );
void FoFUserMessage_GoodBadYou( bf_read &msg );

#endif
