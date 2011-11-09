#include "itkThreeDCircularProjectionGeometry.h"
#include <itkCenteredEuler3DTransform.h>

double itk::ThreeDCircularProjectionGeometry::ConvertAngleBetween0And360Degrees(const double a)
{
  double result = a-360*floor(a/360); // between -360 and 360
  if(result<0) result += 360;         // between 0    and 360
  return result;
}

void itk::ThreeDCircularProjectionGeometry::AddProjection(
  const double sid, const double sdd, const double gantryAngle,
  const double projOffsetX, const double projOffsetY,
  const double outOfPlaneAngle, const double inPlaneAngle,
  const double sourceOffsetX, const double sourceOffsetY)
{
  // Detector orientation parameters
  m_GantryAngles.push_back( ConvertAngleBetween0And360Degrees(gantryAngle) );
  m_OutOfPlaneAngles.push_back( ConvertAngleBetween0And360Degrees(outOfPlaneAngle) );
  m_InPlaneAngles.push_back( ConvertAngleBetween0And360Degrees(inPlaneAngle) );

  // Source position parameters
  m_SourceToIsocenterDistances.push_back( sid );
  m_SourceOffsetsX.push_back( sourceOffsetX );
  m_SourceOffsetsY.push_back( sourceOffsetY );

  // Detector position parameters
  m_SourceToDetectorDistances.push_back( sdd );
  m_ProjectionOffsetsX.push_back( projOffsetX );
  m_ProjectionOffsetsY.push_back( projOffsetY );

  // Projection on the detector normal of SourceToIsocenterDistance
  double sidn = sid;
  if(sourceOffsetX != 0. || sourceOffsetY != 0.)
    sidn = sqrt( sid * sid - sourceOffsetX * sourceOffsetX - sourceOffsetY * sourceOffsetY );

  // Compute sub-matrices
  AddProjectionTranslationMatrix( ComputeTranslationHomogeneousMatrix(projOffsetX-sourceOffsetX, projOffsetY-sourceOffsetY) );
  AddMagnificationMatrix( ComputeProjectionMagnificationMatrix(sdd, sidn) );
  AddRotationMatrix( ComputeRotationHomogeneousMatrix(outOfPlaneAngle, gantryAngle, inPlaneAngle) );
  AddSourceTranslationMatrix( ComputeTranslationHomogeneousMatrix(sourceOffsetX, sourceOffsetY, 0.) );

  Superclass::MatrixType matrix;
  matrix =
    this->GetProjectionTranslationMatrices().back().GetVnlMatrix() *
    this->GetMagnificationMatrices().back().GetVnlMatrix() *
    this->GetRotationMatrices().back().GetVnlMatrix() *
    this->GetSourceTranslationMatrices().back().GetVnlMatrix();

  this->AddMatrix(matrix);
}

const std::multimap<double,unsigned int> itk::ThreeDCircularProjectionGeometry::GetSortedAngles()
{
  unsigned int nProj = this->GetGantryAngles().size();
  std::multimap<double,unsigned int> angles;
  for(unsigned int iProj=0; iProj<nProj; iProj++)
    {
    double angle = this->GetGantryAngles()[iProj];
    angles.insert(std::pair<double, unsigned int>(angle, iProj) );
    }
  return angles;
}

const std::vector<double> itk::ThreeDCircularProjectionGeometry::GetAngularGapsWithNext()
{
  std::vector<double> angularGaps;
  unsigned int        nProj = this->GetGantryAngles().size();
  angularGaps.resize(nProj);

  // Special management of single or empty dataset
  const double degreesToRadians = vcl_atan(1.0) / 45.0;
  if(nProj==1)
    angularGaps[0] = degreesToRadians * 360;
  if(nProj<2)
    return angularGaps;

  // Otherwise we sort the angles in a multimap
  std::multimap<double,unsigned int> angles = this->GetSortedAngles();

  // We then go over the sorted angles and deduce the angular weight
  std::multimap<double,unsigned int>::const_iterator curr = angles.begin(), next = angles.begin();
  next++;

  // All but the last projection
  while(next!=angles.end() )
    {
    angularGaps[curr->second] = degreesToRadians * ( next->first - curr->first );
    curr++; next++;
    }

  //Last projection wraps the angle of the first one
  angularGaps[curr->second] = 0.5 * degreesToRadians * ( angles.begin()->first + 360 - curr->first );

  return angularGaps;
}

const std::vector<double> itk::ThreeDCircularProjectionGeometry::GetAngularGaps()
{
  std::vector<double> angularGaps;
  unsigned int        nProj = this->GetGantryAngles().size();
  angularGaps.resize(nProj);

  // Special management of single or empty dataset
  const double degreesToRadians = vcl_atan(1.0) / 45.0;
  if(nProj==1)
    angularGaps[0] = degreesToRadians * 180;
  if(nProj<2)
    return angularGaps;

  // Otherwise we sort the angles in a multimap
  std::multimap<double,unsigned int> angles = this->GetSortedAngles();

  // We then go over the sorted angles and deduce the angular weight
  std::multimap<double,unsigned int>::const_iterator prev = angles.begin(),
                                                     curr = angles.begin(),
                                                     next = angles.begin();
  next++;

  //First projection wraps the angle of the last one
  angularGaps[curr->second] = 0.5 * degreesToRadians * ( next->first - angles.rbegin()->first + 360 );
  curr++; next++;

  //Rest of the angles
  while(next!=angles.end() )
    {
    angularGaps[curr->second] = 0.5 * degreesToRadians * ( next->first - prev->first );
    prev++; curr++; next++;
    }

  //Last projection wraps the angle of the first one
  angularGaps[curr->second] = 0.5 * degreesToRadians * ( angles.begin()->first + 360 - prev->first );

  return angularGaps;
}

itk::ThreeDCircularProjectionGeometry::ThreeDHomogeneousMatrixType
itk::ThreeDCircularProjectionGeometry::
ComputeRotationHomogeneousMatrix(double angleX,
                                 double angleY,
                                 double angleZ)
{
  const double degreesToRadians = vcl_atan(1.0) / 45.0;

  typedef itk::CenteredEuler3DTransform<double> ThreeDTransformType;
  ThreeDTransformType::Pointer xfm = ThreeDTransformType::New();
  xfm->SetIdentity();
  xfm->SetRotation( angleX*degreesToRadians, angleY*degreesToRadians, angleZ*degreesToRadians );

  ThreeDHomogeneousMatrixType matrix;
  matrix.SetIdentity();
  for(int i=0; i<3; i++)
    for(int j=0; j<3; j++)
      matrix[i][j] = xfm->GetMatrix()[i][j];

  return matrix;
}

itk::ThreeDCircularProjectionGeometry::TwoDHomogeneousMatrixType
itk::ThreeDCircularProjectionGeometry::
ComputeTranslationHomogeneousMatrix(double transX,
                                    double transY)
{
  TwoDHomogeneousMatrixType matrix;
  matrix.SetIdentity();
  matrix[0][2] = transX;
  matrix[1][2] = transY;
  return matrix;
}

itk::ThreeDCircularProjectionGeometry::ThreeDHomogeneousMatrixType
itk::ThreeDCircularProjectionGeometry::
ComputeTranslationHomogeneousMatrix(double transX,
                                    double transY,
                                    double transZ)
{
  ThreeDHomogeneousMatrixType matrix;
  matrix.SetIdentity();
  matrix[0][3] = transX;
  matrix[1][3] = transY;
  matrix[2][3] = transZ;
  return matrix;
}

itk::ThreeDCircularProjectionGeometry::Superclass::MatrixType
itk::ThreeDCircularProjectionGeometry::
ComputeProjectionMagnificationMatrix(double sdd, const double sid)
{
  Superclass::MatrixType matrix;
  matrix.Fill(0.0);
  for(unsigned int i=0; i<3; i++)
    matrix[i][i] = sdd;
  matrix[2][2] = 1.0;
  matrix[2][3] = sid;
  return matrix;
}

const itk::ThreeDCircularProjectionGeometry::HomogeneousVectorType
itk::ThreeDCircularProjectionGeometry::
GetSourcePosition(const unsigned int i) const
{
  HomogeneousVectorType sourcePosition;
  sourcePosition[0] = this->GetSourceOffsetsX()[i];
  sourcePosition[1] = this->GetSourceOffsetsY()[i];
  sourcePosition[2] = -this->GetSourceToIsocenterDistances()[i];
  sourcePosition[3] = 1.;

  // Rotate
  sourcePosition = GetRotationMatrices()[i] * sourcePosition;

  return sourcePosition;
}

const itk::ThreeDCircularProjectionGeometry::ThreeDHomogeneousMatrixType
itk::ThreeDCircularProjectionGeometry::
GetProjectionCoordinatesToFixedSystemMatrix(const unsigned int i) const
{
  // Compute projection inverse and distance to source
  ThreeDHomogeneousMatrixType matrix;
  matrix.SetIdentity();
  matrix[0][3] = -this->GetProjectionTranslationMatrices()[i][0][3];
  matrix[1][3] = -this->GetProjectionTranslationMatrices()[i][1][3];
  matrix[2][3] = this->GetSourceToDetectorDistances()[i] -
                 this->GetSourceToIsocenterDistances()[i];
  matrix[2][2] = 0.; // Force z to axis to detector distance

  // Rotate
  matrix = this->GetRotationMatrices()[i].GetVnlMatrix() * matrix.GetVnlMatrix();
  return matrix;
}

