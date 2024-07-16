#ifndef UIBASE_GAMEFEATURES_GAMEFEATURE_H
#define UIBASE_GAMEFEATURES_GAMEFEATURE_H

#include <typeindex>

namespace MOBase
{

/**
 * Empty class that is inherit by all game features.
 */
class GameFeature
{
public:
  GameFeature()          = default;
#ifdef _WIN32
  virtual ~GameFeature() = 0 {}
#else
  virtual ~GameFeature() = 0;
#endif
  /**
   * @brief Retrieve the type index of the main game feature this feature extends.
   */
  virtual const std::type_info& typeInfo() const = 0;
};

namespace details
{

  template <class T>
  class GameFeatureCRTP : public GameFeature
  {
    const std::type_info& typeInfo() const final { return typeid(T); }
  };

}  // namespace details

}  // namespace MOBase

#endif
