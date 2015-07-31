KNearest::Params params;
params.defaultK=5;
params.isclassifier=true;
//////// Train and find with knearest
Ptr<TrainData> knn;
knn= TrainData::create(AmatOfFeatures,ROW_SAMPLE,AmatOfLabels);
Ptr<KNearest> knn1;
knn1=StatModel::train<KNearest>(knn,params);
knn1->findNearest(AmatOfFeaturesToTest,4,ResultMatOfNearestNeighbours);
/////////////////
