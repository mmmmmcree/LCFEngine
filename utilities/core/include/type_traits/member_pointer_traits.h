#pragma once

#include <type_traits>
#include "concepts/member_pointer_concept.h"

namespace lcf {

template <member_pointer_c MemberPointer>
struct member_pointer_traits;

template <typename Class, typename Member>
struct member_pointer_traits<Member Class::*>
{
    using class_type = Class;
    using member_type = Member;
};

template <member_pointer_c auto member_pointer>
using member_pointer_class_t = member_pointer_traits<decltype(member_pointer)>::class_type; 

template <member_pointer_c auto member_pointer>
using member_pointer_member_t = member_pointer_traits<decltype(member_pointer)>::member_type;


} // namespace lcf