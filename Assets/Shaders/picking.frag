#version 330 core

layout(location = 0) out uint o_ObjectID;

uniform uint u_ObjectID;

void main()
{
    o_ObjectID = u_ObjectID;
}