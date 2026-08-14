#pragma once

#ifdef NDEBUG
static constexpr bool ASSERT_ENABLED = false;
#else
static constexpr bool ASSERT_ENABLED = true;
#endif
