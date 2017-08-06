/*******************************************************************
** This code is part of Breakout.
**
** Breakout is free software: you can redistribute it and/or modify
** it under the terms of the CC BY 4.0 license as published by
** Creative Commons, either version 4 of the License, or (at your
** option) any later version.
******************************************************************/
#include <algorithm>
#include <iostream>
#include <sstream>
#include <math.h>
#include <vector>

#include "game.h"

using namespace std;
using namespace irrklang;
ISoundEngine* SoundEngine = createIrrKlangDevice();

Game::Game(GLuint width, GLuint height)
	: State(GAME_ACTIVE), keys(), Width(width), Height(height)
{

}

Game::~Game()
{
	delete spriteRenderer;
	delete selectionBoxRenderer;
	delete selectionBox;
	for (unsigned int i = 0; i < units.size(); i++)
		delete units[i];
	delete hazardHandler;
}

void Game::Init()
{
	// Load shaders
	ResourceManager::LoadShader("Shaders/sprite.vs", "Shaders/sprite.fs", nullptr, "sprite");
	ResourceManager::LoadShader("Shaders/selectionBox.vs", "Shaders/selectionBox.fs", nullptr, "selectionBox");

	// Configure shaders
	glm::mat4 projection = glm::ortho(0.0f, static_cast<GLfloat>(Width),
		static_cast<GLfloat>(this->Height), 0.0f, -1.0f, 1.0f);
	ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
	ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);
	ResourceManager::GetShader("selectionBox").Use().SetInteger("image", 0);
	ResourceManager::GetShader("selectionBox").SetMatrix4("projection", projection);

	// Set render-specific controls
	spriteRenderer = new SpriteRenderer(ResourceManager::GetShader("sprite"));
	selectionBoxRenderer = new SpriteRenderer(ResourceManager::GetShader("selectionBox"));

	// Load textures
	// second argument asks if the image file has pixels with non-max alpha components
	ResourceManager::LoadTexture("Textures/TempSheep.png", GL_TRUE, "sheep");
	ResourceManager::LoadTexture("Textures/GrassBackground.png", GL_TRUE, "background");
	ResourceManager::LoadTexture("Textures/White.png", GL_FALSE, "selectionBox");
	ResourceManager::LoadTexture("Textures/Lazer.jpg", GL_FALSE, "Lazer");
	ResourceManager::LoadTexture("Textures/LazerExploded.png", GL_FALSE, "LazerExploded");
	ResourceManager::LoadTexture("Textures/Rocket.jpg", GL_FALSE, "Rocket");
	ResourceManager::LoadTexture("Textures/RocketExploded.png", GL_TRUE, "RocketExploded");
	ResourceManager::LoadTexture("Textures/RocketTarget.jpg", GL_FALSE, "RocketTarget");

	/// Set Game Variables
	// sheep
	vector<glm::vec2> locs;
	for (unsigned int i = 0; i < 4; i++)
	{
		for (unsigned int j = 0; j < 3; j++)
		{
			locs.push_back(glm::vec2(Width / 2 + i * 100, Height / 2 + j * 100));
		}
	}
	for (unsigned int i = 0; i < 12; i++)
	{
		units.push_back(new Unit(locs[i], glm::vec2(50, 50),
			ResourceManager::GetTexture("sheep"), glm::vec4(1.0f), true, 0.0f, 100.f));
	}
	// selection box - don't draw it initially
	selectionBox = new Drawable(glm::vec2(0, 0), glm::vec2(0, 0),
		ResourceManager::GetTexture("selectionBox"), glm::vec4(1.0, 1.0, .4, .25), 0.0, false);

	// hazards - test lazer, for now
	hazardHandler = new HazardHandler(SIMPLE, Width, Height,
		ResourceManager::GetTexture("Lazer"), ResourceManager::GetTexture("LazerExploded"),
		ResourceManager::GetTexture("Rocket"), ResourceManager::GetTexture("RocketExploded"), ResourceManager::GetTexture("RocketTarget"));
	hazardHandler->init();
}

void Game::Update(GLfloat dt)
{

	// updating values in units
	for (unsigned int i = 0; i < units.size(); i++)
	{
		//updating unit positions
		units[i]->move(dt);
		
		for (unsigned int j = 0; j < units.size(); j++)
		{
			if (i == j)  // if the unit we're looking at is not the same one we just moved, skip
				continue;
			if (doesPenetrate(units[i], units[j]))
			{
				units[i]->position -= penetrationVector(units[i], units[j]);
				if (!units[j]->moving)
				{
					units[i]->stop();
					units[j]->stop();
				}
			}
		}
	}
	// killing units - must occur at the end of updating because
	// array size and such get modified when a unit is killed
	hazardHandler->update(dt, units);
}

void Game::ProcessInput(GLfloat dt)
{
	// selection input
	if (mbButton == GLFW_MOUSE_BUTTON_LEFT && mbAction == GLFW_PRESS && mbActionPrev == GLFW_RELEASE)
	{
		// place first point of selection box
		selectionBox->position = glm::vec2(mXpos, mYpos);
		selectionBox->size = glm::vec2(0.0);
		selectionBox->bDraw = true;
	}
	if (mbButton == GLFW_MOUSE_BUTTON_LEFT && mbAction == GLFW_PRESS && mbActionPrev == GLFW_PRESS)
	{
		selectionBox->size = glm::vec2(mXpos, mYpos) - selectionBox->position;
	}
	if (mbButton == GLFW_MOUSE_BUTTON_LEFT && mbActionPrev != GLFW_RELEASE && mbAction == GLFW_RELEASE)
	{
		// place second point of selection box - also, stop rendering it
		selectionBox->size = glm::vec2(mXpos, mYpos) - selectionBox->position;
		selectionBox->bDraw = false;
		// for convenience of calculation, I'm gonna force the starting position to be the top-left
		if (selectionBox->size.x < 0)
		{
			selectionBox->size.x *= -1;
			selectionBox->position.x -= selectionBox->size.x;
		}
		if (selectionBox->size.y < 0)
		{
			selectionBox->size.y *= -1;
			selectionBox->position.y -= selectionBox->size.y;
		}


		// select units within bounds
		for (unsigned int i = 0; i < units.size(); i++)
		{
			// if within box, select unit
			if (((units[i]->position.x + units[i]->size.x / 2) > selectionBox->position.x)
				&& ((units[i]->position.x - units[i]->size.x / 2) < selectionBox->position.x + selectionBox->size.x)
				&& ((units[i]->position.y + units[i]->size.y / 2) > selectionBox->position.y)
				&& ((units[i]->position.y - units[i]->size.y / 2) < selectionBox->position.y + selectionBox->size.y))
				units[i]->select();
			//if not within box
			else
			{
				// if not holding shift, deselect
				if (mbMods != GLFW_MOD_SHIFT)
					units[i]->deselect();
			}
		}
	}
	// movement input
	else if (mbButton == GLFW_MOUSE_BUTTON_RIGHT && mbAction == GLFW_PRESS && mbActionPrev == GLFW_RELEASE)
	{
		// only consider units that are selected in the flock stuff
		selectedUnits.clear();
		for (unsigned int i = 0; i < units.size(); i++)
		{
			if (units[i]->selected)
				selectedUnits.push_back(units[i]);
		}
		// use helper function to recreate flocks, which destroys previous flocks
		recreateFlocks(selectedUnits, flocks, Width, Height, 65.f);

		for (unsigned int i = 0; i < flocks.size(); i++)
		{
			flocks[i].setDestination(glm::vec2(mXpos, mYpos));
		}
	}
	if (keys[GLFW_KEY_S])
	{
		for (unsigned int i = 0; i < units.size(); i++)
		{
			if (units[i]->selected)
				units[i]->stop();
		}
	}
	// update previous frame stuff
	mbButtonPrev = mbButton;
	mbActionPrev = mbAction;
	mbModsPrev = mbMods;
}

void Game::Render()
{	
	// draw background
	spriteRenderer->DrawSprite(ResourceManager::GetTexture("background"),
		glm::vec2(Width/2, Height/2), glm::vec2(Width, Height), 0.0f, glm::vec4(1.0f));
	// draw Lazers behind units
	hazardHandler->drawLazers(*spriteRenderer);
	// draw units
	for (unsigned int i = 0; i < units.size(); i++)
		units[i]->draw(*spriteRenderer);
	// draw rockets on top of units
	hazardHandler->drawRockets(*spriteRenderer);
	// draw selection box - low alpha value, so I can draw it over stuff
	// now that I'm rendering from the center of objects, rendering the selection box will probably need to get hacky
	// but this is a one-time thing because I only need one of these
	selectionBox->position.x += selectionBox->size.x / 2;
	selectionBox->position.y += selectionBox->size.y / 2;
	selectionBox->draw(*selectionBoxRenderer);
	selectionBox->position.x -= selectionBox->size.x / 2;
	selectionBox->position.y -= selectionBox->size.y / 2;
}

