/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TNT_FILAMENT_TRANSFORMMANAGER_H
#define TNT_FILAMENT_TRANSFORMMANAGER_H

#include <filament/FilamentAPI.h>

#include <utils/compiler.h>
#include <utils/Entity.h>
#include <utils/EntityInstance.h>

#include <math/mat4.h>

namespace filament {

/**
 * TransformManager is used to add transform components to entities.
 *
 * A Transform component gives an entity a position and orientation in space in the coordinate
 * space of its parent transform. The TransformManager takes care of computing the world-space
 * transform of each component (i.e. its transform relative to the root).
 *
 * Creation and destruction
 * ========================
 *
 * A transform component is created using TransformManager::create() and destroyed by calling
 * TransformManager::destroy().
 *
 * ~~~~~~~~~~~{.cpp}
 *  filament::Engine* engine = filament::Engine::create();
 *  utils::Entity object = utils::EntityManager.get().create();
 *
 *  auto& tcm = engine->getTransformManager();
 *
 *  // create the transform component
 *  tcm.create(object);
 *
 *  // set its transform
 *  auto i = tcm.getInstance(object);
 *  tcm.setTransform(i, mat4f::translate({ 0, 0, -1 }));
 *
 *  // destroy the transform component
 *  tcm.destroy(object);
 * ~~~~~~~~~~~
 *
 */
class UTILS_PUBLIC TransformManager : public FilamentAPI {
public:
    using Instance = utils::EntityInstance<TransformManager>;

    /**
     * Returns whether a particular Entity is associated with a component of this TransformManager
     * @param e An Entity.
     * @return true if this Entity has a component associated with this manager.
     */
    bool hasComponent(utils::Entity e) const noexcept;

    /**
     * Gets an Instance representing the transform component associated with the given Entity.
     * @param e An Entity.
     * @return An Instance object, which represents the transform component associated with the Entity e.
     * @note Use Instance::isValid() to make sure the component exists.
     * @see hasComponent()
     */
    Instance getInstance(utils::Entity e) const noexcept;

    /**
     * Creates a transform component and associate it with the given entity.
     * @param entity            An Entity to associate a transform component to.
     * @param parent            The Instance of the parent transform, or Instance{} if no parent.
     * @param localTransform    The transform to initialize the transform component with.
     *                          This is always relative to the parent.
     *
     * If this component already exists on the given entity, it is first destroyed as if
     * destroy(utils::Entity e) was called.
     *
     * @see destroy()
     */
    void create(utils::Entity entity, Instance parent = {}, const filament::math::mat4f& localTransform = {});

    /**
     * Destroys this component from the given entity, children are orphaned.
     * @param e An entity.
     *
     * @note If this transform had children, these are orphaned, which means their local
     * transform becomes a world transform. Usually it's nonsensical. It's recommended to make
     * sure that a destroyed transform doesn't have children.
     *
     * @see create()
     */
    void destroy(utils::Entity e) noexcept;

    /**
     * Re-parents an entity to a new one.
     * @param i             The instance of the transform component to re-parent
     * @param newParent     The instance of the new parent transform
     * @attention It is an error to re-parent an entity to a descendant and will cause undefined behaviour.
     * @see getInstance()
     */
    void setParent(Instance i, Instance newParent) noexcept;

    /**
     * Returns the parent of a transform component, or the null entity if it is a root.
     * @param i The instance of the transform component to query.
     */
    utils::Entity getParent(Instance i) const noexcept;

    /**
     * Returns the number of children of a transform component.
     * @param i The instance of the transform component to query.
     * @return The number of children of the queried component.
     */
    size_t getChildCount(Instance i) const noexcept;

    /**
     * Gets a list of children for a transform component.
     *
     * @param i The instance of the transform component to query.
     * @param chidren Pointer to array-of-Entity. The array must have at least "count" elements.
     * @param count The maximum number of children to retrieve.
     * @return The number of children written to the pointer.
     */
    size_t getChildren(Instance i, utils::Entity* children, size_t count) const noexcept;

    /**
     * Sets a local transform of a transform component.
     * @param ci              The instance of the transform component to set the local transform to.
     * @param localTransform  The local transform (i.e. relative to the parent).
     * @see getTransform()
     * @attention This operation can be slow if the hierarchy of transform is too deep, and this
     *            will be particularly bad when updating a lot of transforms. In that case,
     *            consider using openLocalTransformTransaction() / commitLocalTransformTransaction().
     */
    void setTransform(Instance ci, const filament::math::mat4f& localTransform) noexcept;

    /**
     * Returns the local transform of a transform component.
     * @param ci The instance of the transform component to query the local transform from.
     * @return The local transform of the component (i.e. relative to the parent). This always
     *         returns the value set by setTransform().
     * @see setTransform()
     */
    const filament::math::mat4f& getTransform(Instance ci) const noexcept;

    /**
     * Return the world transform of a transform component.
     * @param ci The instance of the transform component to query the world transform from.
     * @return The world transform of the component (i.e. relative to the root). This is the
     *         composition of this component's local transform with the it's parent's world
     *         transform.
     * @see setTransform()
     */
    const filament::math::mat4f& getWorldTransform(Instance ci) const noexcept;

    /**
     * Opens a local transform transaction. During a transaction, getWorldTransform() can
     * return an invalid transform until commitLocalTransformTransaction() is called. However,
     * setTransform() will perform significantly better and in constant time.
     *
     * This is useful when updating many transforms and the transform hierarchy is deep (say more
     * than 4 or 5 levels).
     *
     * @note If the local transform transaction is already open, this is a no-op.
     *
     * @see commitLocalTransformTransaction(), setTransform()
     */
    void openLocalTransformTransaction() noexcept;

    /**
     * Commits the currently open local transform transaction. When this returns, calls
     * to getWorldTransform() will return the proper value.
     *
     * @attention failing to call this method when done updating the local transform will cause
     *            a lot of rendering problems. The system never closes the transaction
     *            automatically.
     *
     * @note If the local transform transaction is not open, this is a no-op.
     *
     * @see openLocalTransformTransaction(), setTransform()
     */
    void commitLocalTransformTransaction() noexcept;
};

} // namespace filament

#endif // TNT_TRANSFORMMANAGER_H
