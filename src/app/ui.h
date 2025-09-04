#if !defined(UI_H)
struct EffectUI
{
	const char* name;
	int32 *pIndex;
	int32 maxSize;
	real32 *data1;
	real32 *data2;
	real32 *data3;
	real32 *data4;

	real32* renderScale;
};

void buildUI(EffectUI &effectData)
{
	if (ImGui::Begin("background"))
	{
		ImGui::SliderFloat("Render Scale", effectData.renderScale, 0.3f, 1.f);
		ImGui::Text("Selected effect: %s", effectData.name);
		ImGui::SliderInt("Effect Index", effectData.pIndex, 0, effectData.maxSize);
		ImGui::InputFloat4("data1 ", effectData.data1);
		ImGui::InputFloat4("data2", effectData.data2);
		ImGui::InputFloat4("data3", effectData.data3);
		ImGui::InputFloat4("data4", effectData.data4);
		
	}
	
	ImGui::End();
}
#define UI_H
#endif
