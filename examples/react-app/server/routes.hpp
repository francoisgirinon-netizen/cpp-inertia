#pragma once

// One function per feature group; each registers its Drogon routes.
namespace example {

void registerHome();
void registerUsers();
void registerDashboard();
void registerFeed();
void registerPosts();
void registerHistory();
void registerExternal();

} // namespace example
