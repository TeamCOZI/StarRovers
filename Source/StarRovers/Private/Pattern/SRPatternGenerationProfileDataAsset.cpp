#include "Pattern/SRPatternGenerationProfileDataAsset.h"

USRPatternGenerationProfileDataAsset::USRPatternGenerationProfileDataAsset()
{
	const FSRStellarPatternContract DefaultContract;
	CandidateStellarContracts.Add(DefaultContract);
}
