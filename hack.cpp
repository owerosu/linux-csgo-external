#include "hack.h"

struct iovec g_remote[1024], g_local[1024];
struct hack::GlowObjectDefinition_t g_glow[1024];

int count = 0;
unsigned char spotted = 1;


void hack::Glow(double colors[12], remote::Handle* csgo, remote::MapModuleMemoryRegion* client) {
	if (!csgo || !client)
		return;

	// Reset
	bzero(g_remote, sizeof(g_remote));
	bzero(g_local, sizeof(g_local));
	bzero(g_glow, sizeof(g_glow));

	hack::CGlowObjectManager manager;

	if (!csgo->Read((void*) csgo->m_addressOfGlowPointer, &manager, sizeof(hack::CGlowObjectManager))) {
		Logger::error ("Failed to read glowClassAddress");
		throw 1;
		return;
	}

	size_t count = manager.m_GlowObjectDefinitions.Count;

	void* data_ptr = (void*) manager.m_GlowObjectDefinitions.DataPtr;

	if (!csgo->Read(data_ptr, g_glow, sizeof(hack::GlowObjectDefinition_t) * count)) {
		Logger::error ("Failed to read m_GlowObjectDefinitions");
		throw 1;
		return;
	}

	size_t writeCount = 0;
	unsigned long localPlayer = 0;
	unsigned int teamNumber = 0;

	csgo->Read((void*) csgo->m_addressOfLocalPlayer, &localPlayer, sizeof(long));
	if (localPlayer != 0)
		csgo->Read((void*) (localPlayer+0x128), &teamNumber, sizeof(int));
	
	for (unsigned int i = 0; i < count; i++) {
		if (g_glow[i].m_pEntity != NULL) {
			Entity ent;

			if (csgo->Read(g_glow[i].m_pEntity, &ent, sizeof(Entity))) {
				if (ent.m_iTeamNum != 2 && ent.m_iTeamNum != 3) {
					g_glow[i].m_bRenderWhenOccluded = 0;
					g_glow[i].m_bRenderWhenUnoccluded = 0;
					continue;
				}

				bool entityInCrossHair = false;

				if (localPlayer != 0) {
					if (ent.m_iTeamNum != teamNumber) {
						unsigned int crossHairId = 0;
						unsigned int entityId = 0;
						unsigned int attack = 0x5;
						unsigned int release = 0x4;
						csgo->Read((void*) (localPlayer+0xBBD8), &crossHairId, sizeof(int)); // 0xB390 => 0xB3A0 => 0xB398 => 0xBBD8// +0x7C avec m_bhasdefuser
						csgo->Read((void*) (g_glow[i].m_pEntity + 0x94), &entityId, sizeof(int));

						unsigned int iAlt1Status = 0 ;
						csgo->Read((void*) (csgo->m_addressOfAlt1), &iAlt1Status, sizeof(int));
						if (crossHairId == entityId) {
							entityInCrossHair = true;

							unsigned int shooting;
							csgo->Read((void*) (csgo->m_addressOfForceAttack), &shooting, sizeof(int));

							if (iAlt1Status == 0x5 && shooting != attack) {
								usleep(100);
								csgo->Write((void*) (csgo->m_addressOfForceAttack), &attack, sizeof(int));
								usleep(100);
								csgo->Write((void*) (csgo->m_addressOfForceAttack), &release, sizeof(int));
							}
						}
					}
				}

				csgo->Write((void*) ((unsigned long) g_glow[i].m_pEntity + 0xECD), &spotted, sizeof(unsigned char));

				if (g_glow[i].m_bRenderWhenOccluded == 1)
					continue;

				g_glow[i].m_bRenderWhenOccluded = 1;
				g_glow[i].m_bRenderWhenUnoccluded = 0;

				if (ent.m_iTeamNum == 2 || ent.m_iTeamNum == 3) {
					if (teamNumber == ent.m_iTeamNum) {
						g_glow[i].m_flGlowRed = colors[8];
						g_glow[i].m_flGlowGreen = colors[9];
						g_glow[i].m_flGlowBlue = colors[10];
						g_glow[i].m_flGlowAlpha = colors[11];
					} else {
						g_glow[i].m_flGlowRed = entityInCrossHair ? colors[4] : colors[0];
						g_glow[i].m_flGlowGreen = entityInCrossHair ? colors[5] : colors[1];
						g_glow[i].m_flGlowBlue = entityInCrossHair ? colors[6] : colors[2];
						g_glow[i].m_flGlowAlpha = entityInCrossHair ? colors[7] : colors[3];
					}
				}
			}
		}

		if (csgo->m_bShouldGlow) {
			size_t bytesToCutOffEnd = sizeof(hack::GlowObjectDefinition_t) - g_glow[i].writeEnd();
			size_t bytesToCutOffBegin = (size_t) g_glow[i].writeStart();
			size_t totalWriteSize = (sizeof(hack::GlowObjectDefinition_t) - (bytesToCutOffBegin + bytesToCutOffEnd));

			g_remote[writeCount].iov_base =
				((uint8_t*) data_ptr + (sizeof(hack::GlowObjectDefinition_t) * i)) + bytesToCutOffBegin;
			g_local[writeCount].iov_base = ((uint8_t*) &g_glow[i]) + bytesToCutOffBegin;
			g_remote[writeCount].iov_len = g_local[writeCount].iov_len = totalWriteSize;

			writeCount++;
		}
	}

	process_vm_writev(csgo->GetPid(), g_local, writeCount, g_remote, writeCount, 0);
}
