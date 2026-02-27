set(STATIONCHAT_SOURCES
  protocol/AddBan.hpp
  protocol/AddFriend.hpp
  protocol/AddIgnore.hpp
  protocol/AddInvite.hpp
  protocol/AddModerator.hpp
  protocol/CreateRoom.hpp
  protocol/DestroyAvatar.hpp
  protocol/DestroyRoom.hpp
  protocol/EnterRoom.hpp
  protocol/FailoverReLoginAvatar.hpp
  protocol/FriendStatus.hpp
  protocol/GetAnyAvatar.hpp
  protocol/GetPersistentHeaders.hpp
  protocol/GetPersistentMessage.hpp
  protocol/GetRoom.hpp
  protocol/GetRoomSummaries.hpp
  protocol/IgnoreStatus.hpp
  protocol/KickAvatar.hpp
  protocol/LeaveRoom.hpp
  protocol/LoginAvatar.hpp
  protocol/LogoutAvatar.hpp
  protocol/Protocol.cpp
  protocol/RegistrarGetChatServer.hpp
  protocol/RemoveBan.hpp
  protocol/RemoveFriend.hpp
  protocol/RemoveIgnore.hpp
  protocol/RemoveInvite.hpp
  protocol/RemoveModerator.hpp
  protocol/SendInstantMessage.hpp
  protocol/SendPersistentMessage.hpp
  protocol/SendRoomMessage.hpp
  protocol/SetApiVersion.hpp
  protocol/SetAvatarAttributes.hpp
  protocol/UpdatePersistentMessage.hpp
  protocol/UpdatePersistentMessages.hpp
  ChatAvatar.cpp
  ChatAvatar.hpp
  ChatAvatarService.cpp
  ChatAvatarService.hpp
  ChatEnums.cpp
  ChatEnums.hpp
  Database.hpp
  DatabaseFactory.cpp
  DatabaseFactory.hpp
  DatabaseBootstrap.cpp
  DatabaseBootstrap.hpp
  SqlParameterAdapter.cpp
  SqlParameterAdapter.hpp
  ChatRoom.cpp
  ChatRoom.hpp
  ChatRoomService.cpp
  ChatRoomService.hpp
  GatewayClient.cpp
  GatewayClient.hpp
  GatewayNode.cpp
  GatewayNode.hpp
  main.cpp
  Message.hpp
  PersistentMessage.hpp
  PersistentMessageService.cpp
  PersistentMessageService.hpp
  RegistrarClient.cpp
  RegistrarClient.hpp
  RegistrarNode.cpp
  RegistrarNode.hpp
  StationChatApp.cpp
  StationChatApp.hpp
  StationChatConfig.hpp)

if (STATIONCHAT_WITH_MARIADB)
  list(APPEND STATIONCHAT_SOURCES
    DatabaseMariaDb.cpp
    DatabaseMariaDb.hpp)
endif()

add_executable(stationchat ${STATIONCHAT_SOURCES})

target_link_libraries(stationchat
    stationapi
    ${Boost_LIBRARIES}
    $<$<PLATFORM_ID:Windows>:ws2_32>)


if (STATIONCHAT_WITH_MARIADB)
  target_link_libraries(stationchat PRIVATE MariaDB::Client)
  target_compile_definitions(stationchat PRIVATE STATIONCHAT_WITH_MARIADB=1)
endif()

target_include_directories(stationchat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS stationchat RUNTIME DESTINATION bin)
