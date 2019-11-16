#pragma once
#include "component.h"

class TestComponent :
	public Component
{
public:
	void Initialize() override;
	void Update() override;
	void OnEnable() override;
	void Awake() override;
};
